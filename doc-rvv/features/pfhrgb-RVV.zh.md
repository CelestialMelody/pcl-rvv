# PFHRGB RVV 优化说明

## 当前状态

`features/include/pcl/features/impl/pfhrgb.hpp` 已采纳一个有界 RVV production path（生产路径）：
`PFHRGBEstimation::computePointPFHRGBSignature` 和上游 `computeFeature` 在 RVV 构建中优先尝试
`pcl::pfhrgb_rvv_detail::computePointPFHRGBSignatureRVV`。当前采纳范围是 exact
`pcl::PointXYZRGBNormal -> pcl::PointXYZRGBNormal -> pcl::PFHRGBSignature250`、`nr_split == 5`、
邻域规模不少于 4、AoS xyz / normal / rgb 字段和 KSearch public boundary（公开 K 近邻搜索边界）。
其它模板实例、非 RVV 构建、不满足规模或 bin gate 的情况保持原标量 fallback（回退路径）。

采纳证据来自接入 production 后的板卡 repeated benchmark（重复板卡性能测试）：Milkv-Jupiter，
synthetic PFHRGB rgb-normal grid，`side=32`、`points=1024`、`k=32`、`iterations=8`、`warmup=2`、
5 runs。真实公开入口 `public_pfhrgb_k` 的 B/A 为 `1.28, 1.27, 1.28, 1.27, 1.27`，median `1.27x`，
0/5 低于 1，checksum 一致。Evidence Doctor（证据体检）为 `1 Error / 1 Warning / 6 Suggestions`；
Error 和 Warning 都落在 helper-only / component diagnostic case，不落在 production-public case。

## 函数语义和标量路径

PFHRGB（带颜色的点特征直方图）为一个查询点的邻域计算 250-bin descriptor。`computeFeature` 先通过
KSearch 或 radius search 得到邻域 `nn_indices`，再调用 `computePointPFHRGBSignature`。标量 helper 对邻域内
所有有向点对执行：

- 用两点 xyz 和 normal 调用 `computeRGBPairFeatures`，生成 Darboux frame（局部坐标系）特征
  `f1/f2/f3/f4`。
- 同一 helper 计算 RGB ratio（颜色比例）`f5/f6/f7`。
- `f1/f2/f3` 落到前 125 个 histogram bin，`f5/f6/f7` 落到后 125 个 color bin。
- histogram scatter（直方图离散累加）保持原标量顺序，用固定 `hist_incr` 累加。

当前 RVV 路径只接管 pair tuple（点对特征元组）和 RGB ratio 的批量算术，保留 histogram scatter 的标量顺序，
避免 bin conflict（直方图桶冲突）和非结合浮点累加改变 descriptor 语义。

## 当前采用的优化方式

| 维度 | 当前状态 | 采用或暂缓原因 | 证据 | 边界 / 下一步 |
| --- | --- | --- | --- | --- |
| Std helper | adopted | 原标量主体抽成 `computePointPFHRGBSignatureStd`，让 RVV 分流和 fallback 边界清楚。 | `run_test_compare` | 公开 API 不变。 |
| workspace reuse | adopted | `computeFeature` 在点循环外复用 `PFHRGBPairBatchWorkspace`，避免每个点重复分配 staging / tuple buffers。 | `public_pfhrgb_k` production-public median `1.27x`；reuse diagnostic median `1.24x` | 只在 RVV 构建和 exact 点型 gate 下使用。 |
| pair tuple RVV math | adopted | SoA staging 后用 RVV 计算几何 tuple 和 RGB ratio。 | `check_production_rvv_symbol`、board repeated | helper-only case 自身为负向，采纳依据必须是 public boundary。 |
| histogram scatter | deferred | scatter 有 bin conflict 和顺序累加风险；当前 public speedup 已成立。 | helper/component Doctor 降级；PFH 同类路径也保留标量 scatter | 只有 profile 证明 scatter 是主瓶颈时再恢复。 |
| generic RGB traits | deferred | 当前没有 traits / offset / POD layout 证据覆盖更多 RGB / normal 点型。 | exact fallback test 覆盖非 exact source 保持标量语义 | 需要新建 point-type expansion phase。 |

## VL Chunk 流程

当前 production RVV helper 先把邻域有向点对收集到 SoA staging arrays。每个 VL chunk（可变向量长度分块）
执行：

1. 读取 `p1.xyz`、`p2.xyz`、`n1.normal`、`n2.normal` 和两侧 RGB 分量。
2. 计算 `delta`、距离、Darboux frame 所需的 dot / cross 中间量。
3. 用 `atan2_RVV_f32m2` 等 RVV math helper 生成 `f1/f2/f3`。
4. 对 RGB 分量计算 `f5/f6/f7`，分母为 0 时保持标量路径的 0 ratio 语义。
5. 把 `f1/f2/f3/f5/f6/f7/valid` 写回 workspace，随后按原标量顺序执行 bin clamp 和 histogram scatter。

这样 RVV 改变的是每个邻域内 O(k²) pair math 的执行方式，不改变 pair 顺序、bin clamp、两个 125-bin
区段的布局或输出 descriptor 大小。

## 数值算例

假设一个邻域有 4 个点，有向 pair 顺序包括 `(0,1)`、`(0,2)`、`(0,3)`、`(1,0)` 等 12 个点对。
若当前 VLEN 让 `vl=4`，第一个 chunk 会处理前四个有向 pair：

| lane | p1 index | p2 index | RVV 读取 | 后续标量 scatter |
| --- | ---: | ---: | --- | --- |
| 0 | 0 | 1 | 读取 `cloud[0]` / `cloud[1]` 的 xyz、normal 和 rgb。 | 用 lane 0 的 `f1/f2/f3` 累加前 125 bins，用 `f5/f6/f7` 累加后 125 bins。 |
| 1 | 0 | 2 | 同上。 | 若 pair feature 无效，则跳过。 |
| 2 | 0 | 3 | 同上。 | `hist_incr = 100 / (4*3/2)`，保持原 helper 的归一化口径。 |
| 3 | 1 | 0 | 同上。 | 保持有向 pair 的原遍历顺序。 |

后续 chunk 处理剩余 pair。最终 descriptor 与标量 reference 在测试容差内一致。

## Fallback 矩阵

| 条件 | 行为 | 语义依据 / 证据 |
| --- | --- | --- |
| 非 RVV 构建 | 不编译 RVV helper，公开入口走 Std helper。 | `__RVV10__` 条件编译；Std tests 6/6 pass。 |
| `PointInT` 或 `PointNT` 不是 exact `pcl::PointXYZRGBNormal`，或 `PointOutT` 不是 `PFHRGBSignature250` | 编译期不进入 RVV 分流，继续标量路径。 | `PFHRGBProduction.NonExactSourcePointTypeKeepsScalarFallbackSemantics`。 |
| `nr_split != 5` | RVV helper 返回 `false`，调用 Std helper。 | production helper gate；当前证据只覆盖默认 5x5x5 + 5x5x5。 |
| `indices.size() < 4` | RVV helper 返回 `false`，调用 Std helper。 | 小规模邻域下 RVV overhead 不确定。 |
| histogram scatter | 保持标量。 | 避免 bin conflict 和累加顺序风险。 |

## Bench 与证据

| 证据 | 命令 / 路径 | 结果 | 能证明什么 |
| --- | --- | --- | --- |
| correctness | `make -B -C test-rvv/features/pfhrgb run_test_compare` | Std 6/6、RVV 6/6 pass | scalar reference、production exact path、fallback 和 diagnostic wrappers 在 QEMU 正确性范围内一致。 |
| asm attribution（反汇编归属） | `make -B -C test-rvv/features/pfhrgb check_production_rvv_symbol` | `computePointPFHRGBSignatureRVV` 出现在 RVV bench full asm；RVV 指令存在。 | 接入后的 production helper 进入 RVV binary。 |
| board repeated | `test-rvv/features/pfhrgb/log/board/repeated/evidence_manifest.json` | `public_pfhrgb_k` median `1.27x`，0/5 低于 1。 | 目标板卡上当前 public RVV path 快于当前 public scalar path。 |
| Evidence Doctor | `test-rvv/features/pfhrgb/log/board/repeated/evidence_doctor.md` | `1E/1W/6S` | helper-only / component claims 降级；production-public case 无阻塞异常。 |
| evidence registry | `test-rvv/features/pfhrgb/log/evidence_registry.json` | summary-only evidence fresh | 文档引用的 manifest / Doctor 已登记。 |

QEMU 只作为 correctness（正确性）、构建和路径证据；性能结论只来自板卡 repeated summary。

## 正确性与高效性证据链

| 层级 | 当前证据 | 结论 | 不能外推的范围 |
| --- | --- | --- | --- |
| correctness | `run_test_compare`，含 production exact path 与非 exact source fallback 测试。 | exact `PointXYZRGBNormal` public path descriptor 与标量 reference 一致。 | 不证明泛型 RGB traits、其它输出类型或所有 fallback 原因。 |
| path / asm | production helper 符号和 RVV 指令归属。 | 编译后的 RVV 构建确实包含目标 RVV helper。 | 不单独证明热点占比或非 exact 模板实例。 |
| performance | 接入后 board repeated。 | `public_pfhrgb_k` 为 stable positive，median `1.27x`。 | 不证明其它目标硬件、其它规模、radius search 或真实数据集。 |
| boundary | exact gate + fallback tests。 | 未覆盖点型保持标量，不改变 public API。 | exact gate 不是最终泛型策略。 |
| risk | Doctor Error 不在 production-public case。 | 当前可采纳；helper-only 退化不作为回滚依据。 | metadata 缺 taskset / governor / freq / temperature，降低环境复盘能力。 |

## Traceability Map

| 符号 / 文件 | 层级 | 作用 | 调用者 / 上游入口 | 被调用者 / 下游消费者 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- | --- | --- |
| `PFHRGBEstimation::computeFeature` | production public entry | 搜索邻域、复用 RVV workspace 并输出 descriptor。 | `Feature::compute()` | RVV helper 或 Std helper | production-public evidence | `features/include/pcl/features/impl/pfhrgb.hpp` |
| `computePointPFHRGBSignatureStd` | production Std helper | 保留原标量 descriptor 语义。 | public dispatch / fallback | `computeRGBPairFeatures` | fallback source | `features/include/pcl/features/impl/pfhrgb.hpp` |
| `computePointPFHRGBSignatureRVV` | production RVV helper | 批量计算 pair tuple 和 RGB ratio。 | `computeFeature` / exact dispatch | RVV math helper、scalar scatter | adopted RVV path | `features/include/pcl/features/impl/pfhrgb.hpp` |
| `test_pfhrgb.cpp` | correctness tests | reference、production exact path、fallback 和 diagnostic wrappers 对拍。 | `make run_test_compare` | test support helper | correctness gate | `test-rvv/features/pfhrgb/src/test_pfhrgb.cpp` |
| `bench_pfhrgb.cpp` | bench wrapper | 输出 component、diagnostic 和 public case timing / checksum。 | `board_repeated` | manifest script | board performance input | `test-rvv/features/pfhrgb/src/bench_pfhrgb.cpp` |
| `generate_pfhrgb_evidence_manifest.py` | analysis script | 将 repeated board summary 转成 Evidence Doctor manifest。 | `make evidence_doctor_repeated` | `evidence_doctor.py` | doctor / registry input | `test-rvv/features/pfhrgb/script/generate_pfhrgb_evidence_manifest.py` |
| Phase 020 result | phase result | 保存 PI1-PI5 接入、测试、板卡和采纳结论。 | phase loop | 本文、roadmap、matrix、Handoff | production adoption source | `test-rvv/features/pfhrgb/doc/phases/020-pi1-production-integration-plan/result.zh.md` |
| `pfhrgb-evaluation.zh.md` | topic-local evaluation | 保存函数级评估、production patch scope、fallback 和 EvidenceDecision。 | README / phase loop | 本文和 phase result | decision audit | `test-rvv/features/pfhrgb/doc/pfhrgb-evaluation.zh.md` |

## 生产接入后的 closeout

| 项 | 最终状态 | 证据 |
| --- | --- | --- |
| 生产补丁范围 | adopted；只改 `features/include/pcl/features/impl/pfhrgb.hpp`，未改变 public API。 | Phase 020 result、当前源码。 |
| 覆盖范围 | exact `PointXYZRGBNormal`、`PFHRGBSignature250`、`nr_split=5`、KSearch public boundary。 | fallback matrix、production helper gate。 |
| 不覆盖范围 | 泛型 RGB / normal traits、自定义点型、非默认 bins、小规模 RVV、其它目标硬件。 | Phase 020 result、optimization matrix。 |
| production evidence | `public_pfhrgb_k` 5-run median `1.27x`。 | 接入后 board repeated manifest。 |
| 诊断到生产结论变化 | Phase 030 reuse diagnostic 用于选择 workspace reuse；最终采纳以 Phase 020 production-public 为准。 | Phase 030 / Phase 020 result。 |
| 回退策略 | helper 返回 `false` 或 exact gate 不满足后继续 Std helper；非 RVV 构建没有 RVV helper。 | source gate 与 Std/RVV tests。 |

## 后续方向

当前不建议在同一 exact-gated boundary 继续微调。helper-only case 在当前 repeated board 中为负向，
histogram scatter 仍有顺序语义风险，继续优化缺少 profile 支撑。若后续需要扩大收益覆盖，优先新建
point-type expansion phase：按 PCL traits（点类型字段特征）、字段 offset、POD layout、normal / rgb 字段
和 fallback matrix 证明更多点型可安全接入，再单独跑 correctness、ASM、board repeated 和 Evidence Doctor。
