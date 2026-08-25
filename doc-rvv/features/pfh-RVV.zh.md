# PFH RVV 优化说明

## 当前状态

`features/include/pcl/features/impl/pfh.hpp` 已采纳一个有界 RVV production path（生产路径）：
`PFHEstimation::computePointPFHSignature` 在 RVV 构建中优先尝试
`pcl::detail::computePointPFHSignatureDirectAoSRVV`。当前采纳范围是 exact
`pcl::PointNormal -> pcl::PointNormal` 和 exact `pcl::PointXYZ -> pcl::Normal`、
`nr_split == 5`、`use_cache_ == false`、邻域规模不少于 4、source / normal cloud 均为
AoS float 布局，且 32-bit byte offset 可以表达所有访问。其它模板实例和运行条件保持原标量路径。

采纳证据来自接入 production 后的板卡 repeated benchmark（重复板卡性能测试）：Milkv-Jupiter，
synthetic PFH PointNormal grid，`side=32`、`points=1024`、`k=32`、`iterations=8`、
`warmup=2`、5 runs。`component_pfh_signature` mean 为 `2.004x`，`public_pfh_k` mean 为
`1.926x`；Evidence Doctor（证据体检）为 `0 Errors / 0 Warnings / 8 Suggestions`，suggestions
仅要求补充环境 metadata 和 binary hash，不阻塞当前 positive bucket（正向决策桶）。

`PointXYZ + Normal` 的采纳证据来自独立 Phase 060 接入后板卡 repeated：`component_pfh_signature_pointxyz_normal`
mean 为 `1.922x`，`public_pfh_pointxyz_normal_k` mean 为 `1.854x`；Evidence Doctor 为
`0 Errors / 0 Warnings / 12 Suggestions`，suggestions 同样只涉及环境 metadata 和 binary hash。

## 函数语义和标量路径

PFH（Point Feature Histogram，点特征直方图）为一个查询点的邻域计算 125-bin descriptor。
`computeFeature` 先通过 KSearch 或 radius search 得到邻域 `nn_indices`，再调用
`computePointPFHSignature`。该 helper 对邻域内所有无序点对执行 `computePairFeatures`：

- 用两点 xyz 计算 `delta` 和距离 `f4`。
- 用两侧 normal 与 `delta` 的夹角选择 Darboux frame（局部坐标系）。
- 生成 `f1`、`f2`、`f3` 三个特征值。
- 按 `nr_split` 把三维特征落到 histogram bin，再用固定 `hist_incr` 累加。

`use_cache_ == true` 时，标量路径会维护 pair feature cache（点对特征缓存）。当前 RVV 路径不接管
cache 状态；命中 cache path 时直接回退到原标量实现。

## 当前采用的优化方式

| 维度 | 当前状态 | 采用或暂缓原因 | 证据 | 边界 / 下一步 |
| --- | --- | --- | --- | --- |
| dispatch（分流逻辑） | adopted | 公开入口先做原有 histogram 初始化，再在 `__RVV10__` 下短路尝试 RVV helper；helper 返回 `false` 时继续原标量主体。 | `features/include/pcl/features/impl/pfh.hpp`、`run_test_compare` | 后续若拆出独立 Std helper，可作为实现整理 phase，不改变当前语义。 |
| direct AoS gather（直接结构数组离散加载） | adopted | 不再把点对字段复制到 SoA staging buffer，而是用 byte offset gather 直接读取 xyz 和 normal，减少额外内存流量。 | Phase 040 / 060 board repeated；production asm 含 `vluxei32.v` | 当前只覆盖 exact `PointNormal -> PointNormal` 和 exact `PointXYZ -> Normal`。 |
| pair tuple math（点对特征数学） | adopted | `dist`、normal dot、frame cross、`atan2_RVV_f32m2` 和 histogram 前的 `f1/f2/f3` 生成在 VL chunk 内完成。 | RVV helper asm 含 `vfmacc.vv`、`vfsqrt.v`、`vfdiv.vv` | histogram scatter 仍保留标量，避免并发 bin conflict（直方图冲突累加）改变语义。 |
| staged SoA family | deferred | Phase 030 诊断也正向，但 direct AoS 更快且生产维护边界更小。 | Phase 030 / 040 phase result | 只有 direct AoS 后续点型扩展不可维护或被回滚时再恢复。 |
| `PointXYZ + Normal` | adopted | 这是常见 PFH 组合；Phase 060 已独立补 production direct correctness、asm、board repeated 和 Doctor。 | Phase 060 result；component mean `1.922x`，public mean `1.854x` | 只覆盖 exact `pcl::PointXYZ -> pcl::Normal`，不外推到泛型 traits。 |

## VL chunk 流程

RVV helper 先把邻域 all-pairs 变成四组 32-bit byte offset：`p1`、`p2`、`n1`、`n2`。每个 VL chunk
执行：

1. 用 `indexed_load3_fields_f32m2` 从 point cloud 读取 `p1.xyz` 和 `p2.xyz`。
2. 用同一类 gather helper 从 normals cloud 读取 `n1.normal` 和 `n2.normal`。
3. 计算 `delta`、距离、两侧 normal 投影和 swap mask（交换掩码）。
4. 按标量 Darboux frame 规则生成 `f1/f2/f3`，并记录有效 lane。
5. 把 `f1/f2/f3/valid` 写到连续 staging arrays，随后按原标量顺序执行 bin clamp 和 histogram scatter。

这条路径使用 FMA（fused multiply-add，融合乘加）组织点积和平方和。correctness 对拍允许
`2e-3f` 误差，原因是 RVV 数学 helper 和 FMA contraction 可能改变少量中间舍入；最终 histogram
与生产标量 helper 在该误差预算内一致。

## 数值算例

假设一个邻域有 4 个点，all-pairs 顺序为 `(1,0)`、`(2,0)`、`(2,1)`、`(3,0)`、`(3,1)`、`(3,2)`。
若当前 VLEN 让 `vl=4`，第一个 chunk 会处理前四个 pair：

| lane | p1 index | p2 index | RVV 读取 | 后续标量 scatter |
| --- | ---: | ---: | --- | --- |
| 0 | 1 | 0 | 读取 `cloud[1].xyz`、`cloud[0].xyz`、`normals[1].normal`、`normals[0].normal` | 用 lane 0 的 `f1/f2/f3` 计算 bin。 |
| 1 | 2 | 0 | 同上，offset 分别来自 `2*sizeof(PointT)` 和 `0` | 保持 all-pairs 原顺序累加。 |
| 2 | 2 | 1 | 同上 | 若距离或 frame norm 为 0，则 `valid=0` 跳过。 |
| 3 | 3 | 0 | 同上 | 累加 `hist_incr = 100 / 6`。 |

第二个 chunk 处理剩余两个 pair。这样 RVV 只改变 `f1/f2/f3` 的批量计算方式，不改变 pair 顺序、
bin clamp 或 histogram 的累加顺序。

## Fallback 矩阵

| 条件 | 行为 | 语义依据 / 证据 |
| --- | --- | --- |
| 非 RVV 构建 | 不编译 RVV helper，公开入口走原标量路径。 | `__RVV10__` 条件编译；Std tests 3/3 pass。 |
| `PointInT` / `PointNT` 不是 exact `PointNormal -> PointNormal` 或 exact `PointXYZ -> Normal` | RVV helper compile-time 返回 `false`，公开入口继续标量主体。 | Phase 040 / 060 窄范围 gate；泛型 traits 未外推。 |
| source 布局不满足 `RVVXYZAoSFloatLayout`，或 normals 布局不满足 PFH 本地 normal AoS gate | 返回 `false`。 | traits / layout gate 分别证明 source xyz 和 normals normal 的单 float AoS offset。 |
| `use_cache_ == true` | 公开入口不调用 RVV helper。 | cache 更新顺序和状态保持标量。 |
| `nr_split != 5` 或 `indices.size() < 4` | 返回 `false`。 | 当前证据只覆盖 PFHSignature125 默认 5x5x5 和足够点对。 |
| cloud / normals 规模超出 32-bit byte offset | 返回 `false`。 | gather helper 使用 32-bit byte offset。 |
| index 为负、越界，或 normals 与 cloud 尺寸不覆盖 index | 返回 `false`，由标量路径保留既有行为。 | Phase 040 fallback gate。 |
| 非 finite point 或退化 pair | 非 finite point 跳过；距离或 frame norm 为 0 的 lane 标记无效并跳过 scatter。 | `run_test_compare` 与 production helper 测试。 |

## Bench 与证据

| 证据 | 命令 / 路径 | 结果 | 能证明什么 |
| --- | --- | --- | --- |
| correctness | `make -B -C test-rvv/features/pfh run_test_compare` | Std 3/3、RVV 6/6 pass | 标量参考、diagnostic candidate、两个 production helper 点型组合和 fallback 在 QEMU 正确性范围内一致。 |
| asm attribution（反汇编归属） | `make -B -C test-rvv/features/pfh dump_bench_rvv` | 两个 production helper 实例含 `vluxei32.v`、`vfmacc.vv`、`vfsqrt.v`、`vfdiv.vv` | RVV 指令归属到 PFH production helper。 |
| board repeated: PointNormal | `test-rvv/features/pfh/log/board/pi2-production-direct-aos/repeated` | component `2.004x` mean；public `1.926x` mean | 目标板卡上 exact `PointNormal -> PointNormal` production detail 和 public KSearch 入口均正向。 |
| board repeated: PointXYZ + Normal | `test-rvv/features/pfh/log/board/pi3-pointxyz-normal/repeated` | component `1.922x` mean；public `1.854x` mean | 目标板卡上 exact `PointXYZ -> Normal` production detail 和 public KSearch 入口均正向。 |
| Evidence Doctor | Phase 040 / 060 repeated doctor reports | Phase 040 `0E/0W/8S`；Phase 060 `0E/0W/12S` | repeated summary 无阻塞异常；metadata 建议不改变采纳判断。 |
| evidence registry | `test-rvv/features/pfh/log/evidence_registry.json` | Phase 040 / 060 summary fresh | 文档引用的 summary / doctor / manifest 已登记。 |

QEMU 只作为 correctness（正确性）、构建和路径证据；性能结论只来自板卡 repeated summary。

## 正确性与高效性证据链

| 层级 | 当前证据 | 结论 | 不能外推的范围 |
| --- | --- | --- | --- |
| correctness | `run_test_compare`，含 production RVV helper gtest | exact `PointNormal -> PointNormal` 和 exact `PointXYZ -> Normal` 下 histogram 与标量路径在误差预算内一致。 | 不证明泛型 traits、自定义点型或 cache path。 |
| path / asm | production helper 符号和 RVV 指令归属 | 编译后的 RVV 构建确实包含目标 gather、FMA、sqrt、div 指令。 | 不单独证明热点占比或性能。 |
| performance | Phase 040 和 Phase 060 接入后 board repeated | 两个 exact 点型组合的 production-detail 和 production-public 均为 positive。 | 不证明其它目标硬件、其它规模或其它点型。 |
| boundary | fallback matrix + exact gate | 未覆盖路径保持标量，不改变 public API。 | exact gate 不是最终泛型策略。 |
| risk | Doctor 仅有 metadata suggestions | 当前可采纳；后续扩大范围应补 taskset / governor / freq / temperature / binary hash。 | metadata 缺失降低环境复盘能力，但不推翻稳定正向桶。 |

## Traceability Map

| 符号 / 文件 | 层级 | 作用 | 调用者 / 上游入口 | 被调用者 / 下游消费者 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- | --- | --- |
| `computePointPFHSignature` | production public entry | 初始化 histogram，并在 RVV 构建中尝试 direct AoS helper。 | `computeFeature` | RVV helper 或原标量 pair loop | production boundary | `features/include/pcl/features/impl/pfh.hpp` |
| `pcl::detail::computePointPFHSignatureDirectAoSRVV` | production RVV helper | 构造 pair offsets，批量计算 `f1/f2/f3`，再标量 scatter。 | `computePointPFHSignature` | `pcl::atan2_RVV_f32m2`、rvv point load helpers | production direct correctness / asm / board | `features/include/pcl/features/impl/pfh.hpp` |
| `test_pfh.cpp` | correctness tests | same-chain reference、candidate 和 production helper 对拍。 | `make run_test_compare` | test-support helper | correctness gate | `test-rvv/features/pfh/src/test_pfh.cpp` |
| `bench_pfh.cpp` | bench wrapper | 输出 component、candidate 和 public case timing / checksum。 | `make run_board_bench_compare`、`board_repeated` | PFH public entry 和 test candidates | board performance input | `test-rvv/features/pfh/src/bench_pfh.cpp` |
| `generate_pfh_evidence_manifest.py` | analysis script | 将 repeated board summary 转成 Evidence Doctor manifest。 | `make evidence_doctor_repeated` | `evidence_doctor.py` | doctor / registry input | `test-rvv/features/pfh/script/generate_pfh_evidence_manifest.py` |
| Phase 040 result | phase result | 保存 PI2-PI5 生产探针、接入后板卡和 PI5 决策。 | phase loop | 本文、roadmap、matrix、Handoff | production adoption source | `test-rvv/features/pfh/doc/phases/040-direct-aos-production-probe/result.zh.md` |
| Phase 050 result | phase result | 保存用户确认采纳后的 S11 文档收口和下一 phase 入口。 | phase loop | Handoff、roadmap、matrix | closeout source | `test-rvv/features/pfh/doc/phases/050-production-closeout-doc-rvv/result.zh.md` |
| Phase 060 result | phase result | 保存 `PointXYZ + Normal` 生产扩展、接入后板卡和停止判断。 | phase loop | 本文、roadmap、matrix、Handoff | production adoption source | `test-rvv/features/pfh/doc/phases/060-pointxyz-normal-production-expansion/result.zh.md` |
| `pfh-evaluation.zh.md` | topic-local evaluation | 保存函数级评估、production patch scope、fallback 和 EvidenceDecision。 | README / phase loop | 本文和 phase results | decision audit | `test-rvv/features/pfh/doc/pfh-evaluation.zh.md` |

## 生产接入后的 closeout

| 项 | 最终状态 | 证据 |
| --- | --- | --- |
| 生产补丁范围 | adopted；只改 `features/include/pcl/features/impl/pfh.hpp`，未改变 public API。 | Phase 040 result、当前源码。 |
| 覆盖范围 | exact `PointNormal -> PointNormal` 与 exact `PointXYZ -> Normal`、float AoS、`nr_split=5`、`use_cache_=false`。 | fallback matrix、production helper gate。 |
| 不覆盖范围 | 泛型 point traits、自定义点型、cache path、OMP path、非默认 bins、其它目标硬件。 | Phase 040 / 050 / 060 phase docs、optimization matrix。 |
| production direct evidence | PointNormal component mean `2.004x`、public mean `1.926x`；PointXYZ+Normal component mean `1.922x`、public mean `1.854x`。 | Phase 040 / Phase 060 repeated board。 |
| 诊断到生产结论变化 | Phase 030 diagnostic 正向用于选择 direct AoS；最终采纳以 Phase 040 production direct 为准。 | Phase 030 / 040 result。 |
| 回退策略 | helper 返回 `false` 后继续原标量 loop；非 RVV 构建没有 RVV helper。 | source gate 与 Std/RVV tests。 |

## 后续方向

当前不建议在同一 topic 继续扩大生产补丁。最有价值的 exact 点型扩展 `PointXYZ + Normal` 已在
Phase 060 采纳；继续到 PointXYZ-like / Normal-like 泛型 traits 需要公共 normal AoS gate、更多点型
编译 / 运行证据和更宽 fallback 矩阵，适合作为新 phase 或后续 topic。cache path 和 OMP path 的语义状态
不同，应等 profile 指向后另开窄 phase。histogram scatter 或 125-bin output copy 相比 O(k^2) pair math
占比小，且 scatter 有 bin conflict 语义风险，当前没有证据显示值得优先接入。
