# PPF RVV 生产优化说明

## 当前状态

`features/include/pcl/features/impl/ppf.hpp` 已采纳一个有界 RVV（RISC-V Vector，可伸缩向量）
production path（生产路径）。公开 API（公开接口）不变；RVV 代码只在 `__RVV10__` 构建中编译。
`PPFEstimation::computeFeature` 会先尝试 traits-gated（基于字段特征门控）的 source xyz AoS
（source 点的 xyz 结构数组布局）+ normal AoS + exact `pcl::PPFSignature` 分流，命中后由
`pcl::detail::computePPFFeatureAlphaMRVV` 批量计算 `alpha_m`。非 RVV 构建、不满足 traits / AoS layout
或输出类型不是 `PPFSignature` 的路径继续走
`pcl::detail::computePPFFeatureStd` 标量 fallback（回退路径）。

当前采用范围只覆盖 ordered `indices_ x input_` all-pairs（有序索引行乘完整输入点云的全部点对）
输出、float AoS（结构数组）布局，以及 exact `PPFSignature` 输出。代表性板卡覆盖包括
`PointXYZ + Normal`、`PointXYZI + Normal` 和 `PointXYZ + PointNormal`。`f1..f4` 仍使用
生产源码原本调用的 `pcl::computePairFeatures`，没有替换为 `pcl::computePPFPairFeature`。Phase 010
的 pair-feature batch RVV 候选已因板卡负向证据拒绝进入 production。

采纳证据来自接入 production 后的 Milkv-Jupiter board repeated benchmark（板卡重复性能测试）。
Phase 060 的 production-public（生产公开入口）代表性点型 case 使用 synthetic ppf grid，`side=28`、
`points=784`、`indices=64`、`repeat=8`、`iterations=8`、`warmup=2`、5 runs：

- `PointXYZI + Normal -> PPFSignature`：speedup `1.40, 1.43, 1.33, 1.41, 1.35`；
  mean Std `429.5662 ms`，mean RVV `310.3166 ms`。
- `PointXYZ + PointNormal -> PPFSignature`：speedup `1.33, 1.33, 1.31, 1.33, 1.35`；
  mean Std `412.4258 ms`，mean RVV `310.2756 ms`。

两组 Evidence Doctor（证据体检）均为 `Errors=0, Warnings=0, Suggestions=2`；suggestions 只涉及环境
metadata（元数据）和 binary identity（二进制身份）缺失，不阻塞当前 positive bucket（正向决策桶）。

## 函数语义和标量路径

PPF（Point Pair Feature，点对特征）为 model reference point（模型参考点）和 model point（模型点）
之间生成 `PPFSignature`。`PPFEstimation::computeFeature` 是 `compute()` 公开流程里的实际计算体：
它先把输出 resize 到 `indices_->size() * input_->size()`，再按 `indices_` 外层、完整 `input_` 内层
顺序写 `output[index_i * input_->size() + j]`。

标量路径对每个非 identity pair（自身点对）执行两段计算：

- 调用 `pcl::computePairFeatures`，从两点 xyz 和两侧 normal 生成 `f1/f2/f3/f4`。这是当前
  `impl/ppf.hpp` 的真实生产 helper choice（辅助函数选择），因此 RVV 证据不能改用
  `computePPFPairFeature` 作为替代语义。
- 构造 `Eigen::AngleAxisf` 和 `Eigen::Affine3f`，把参考 normal 旋转到 x 轴方向附近，再把 model point
  变换到该坐标系下，用 `atan2(-z, y)` 生成 `alpha_m`。

identity pair 写五个 NaN，并把 `output.is_dense=false`。`computePairFeatures` 返回失败时也写 NaN、
标记非 dense，并保留原有 `PCL_ERROR` 行为。

## 当前采用的优化方式

| 维度 | 当前状态 | 采用或暂缓原因 | 证据 | 边界 / 下一步 |
| --- | --- | --- | --- | --- |
| production dispatch（生产分流） | adopted | 公开入口保持短分流：RVV helper 成功则返回，否则调用 Std helper。 | Phase 040 exact case；Phase 060 point-type expansion cases。 | 公开 API 不变；非覆盖模板实例回退标量。 |
| Std helper | adopted | 原标量主体抽到 `computePPFFeatureStd`，让 fallback 边界可审查。 | Std/RVV correctness 对拍。 | 保持原 `computePairFeatures` 与 Eigen alpha 链路。 |
| `alpha_m` RVV batch | adopted | Phase 020 证明 closed-form（闭式公式）与 Eigen reference 对齐；Phase 030 诊断正向；Phase 040 / 060 生产公开入口仍正向。 | Phase 060 新代表点型 speedup mean 约 `1.384x` 和 `1.33x`。 | 只覆盖 `alpha_m` 后段；`f1..f4` 仍标量 helper。 |
| pair-feature batch RVV | rejected | SoA-staged pair-feature 候选 correctness 成立，但 board repeated 5/5 退化。 | Phase 010 speedup `0.80, 0.79, 0.79, 0.79, 0.81`。 | 不进入 production；只有新 profile 证明可消除 staging 退化时再恢复。 |
| generic point traits | adopted for current gate | Phase 060 使用公共 source xyz AoS traits 和 PPF 本地 normal AoS gate，且以 `PointXYZI + Normal`、`PointXYZ + PointNormal` 代表性 case 完成 correctness / asm / board / Doctor。 | Phase 060 result 和 optimization matrix。 | 其它自定义 traits-compatible 点型是 gate-allowed but not individually board-covered；`Scalar=double` 和非 `PPFSignature` 不覆盖。 |
| environment / binary metadata hardening | deferred | Evidence Doctor suggestion 不阻塞当前正向桶，但会增强归档可复核性。 | Evidence Doctor `0E/0W/2S`。 | 只有提交严格证据归档或出现长尾 / 方向反转时优先补。 |

## RVV 数据流与 VL Chunk 流程

生产 RVV helper 仍先按标量顺序遍历 `indices_ x input_`。对每个非 identity pair，它调用
`pcl::computePairFeatures` 计算 `f1..f4` 和失败语义；只有 helper 成功的点对才把 `dx/dy/dz`、
参考 normal 的 `nx/ny/nz` 和输出行号写入连续 staging arrays（分阶段暂存数组）。

随后每个 VL chunk（可变向量长度分块）执行：

1. 连续加载 `dx/dy/dz` 和 `nx/ny/nz`。
2. 用 `ny*ny + nz*nz` 构造 `parallel_to_x` mask（掩码）；参考 normal 与 x 轴平行时走原标量特殊轴语义。
3. 对一般 normal 使用闭式旋转公式计算变换后的 `y/z`；对平行 x 轴路径使用专门公式。
4. 调用 `pcl::atan2_RVV_f32m2` 计算 `atan2(-transformed_z, transformed_y)`，并写回 `alpha_m`。
5. 按 staging 中保存的 `output_rows` 写回对应 `PPFSignature::alpha_m`；identity 和失败 pair 的 NaN 行保持不变。

这条路径把原标量里每个点对的 Eigen transform 对象构造和 `alpha_m` 后段数学替换为批量 RVV 公式。它没有改变
输出顺序、identity NaN、`computePairFeatures` 的失败处理，也没有改变 `PPFSignature` 字段布局。

## 数值算例

假设参考点 `p_i=(1, 0, 0)`、目标点 `p_j=(1, 2, 3)`，参考 normal 为 `n_i=(1, 0, 0)`。
标量路径把 normal 判为与 x 轴平行，使用 y 轴作为旋转轴；在该特殊路径下：

```text
delta = p_j - p_i = (0, 2, 3)
transformed_y = dy = 2
transformed_z = dz = 3
alpha_m = atan2(-3, 2) ~= -0.982794
```

RVV helper 在一个 VL chunk 中把该点对作为某个 lane（向量通道）处理。`parallel_to_x` mask 对这个 lane
为 true，因此 `transformed_y` 从 `vdy` 选择 2，`transformed_z` 从 parallel formula 选择 3，
再由 `atan2_RVV_f32m2` 给出同一方向的 `alpha_m`。其它 lane 可以同时处理不同的点对，但写回仍使用
`output_rows` 保持原 `index_i * input_size + j` 顺序。

## Fallback 矩阵

| 条件 | 行为 | 语义依据 / 证据 |
| --- | --- | --- |
| 非 `__RVV10__` 构建 | 不编译 RVV helper；`computeFeature` 直接调用 Std helper。 | 条件编译；Std correctness 5/5 pass。 |
| `PointInT` 不满足 `RVVXYZAoSFloatLayout` | 编译期不进入 RVV helper，回退 Std。 | PCL traits 证明 source xyz 单 float 字段、POD standard-layout、stride 和 field offset；不满足则 fallback。 |
| `PointNT` 不满足 PPF normal AoS gate | 编译期回退 Std。 | PPF 本地 gate 复用 `RVVNormalFloatLayout` 并检查 POD standard-layout、`sizeof` 和 normal field offset。 |
| `PointOutT` 不是 exact `pcl::PPFSignature` | 编译期回退 Std。 | 当前 RVV 直接写 `f1..f4/alpha_m` 字段。 |
| identity pair | 写五个 NaN，并标记 `output.is_dense=false`。 | `PPFReference.MarksIdentityPairsAsNaNAndNotDense`。 |
| `computePairFeatures` 失败 | 写五个 NaN，保留 `PCL_ERROR`，并标记非 dense。 | Std helper 和 RVV helper 共享同一 failure 处理边界。 |
| `Scalar=double` 或其它字段类型 / layout | 回退 Std 或不进入当前模板实例。 | 当前 `PPFSignature` 字段是 float；未做 double production evidence。 |
| source-indexed、dual-indexed、correspondence row source | 不属于当前 `PPFEstimation::computeFeature` 生产补丁范围。 | roadmap / matrix 标为未覆盖；需另开 phase 或 topic。 |

## 标量路径与 RVV 路径差异

| 标量阶段 | RVV 阶段 | 保留差异 |
| --- | --- | --- |
| 初始化输出大小、height、width 和 dense 状态。 | 相同。 | 输出布局不变。 |
| 外层遍历 `indices_`，内层遍历完整 `input_`。 | 相同。 | row source 不扩大。 |
| identity pair 写 NaN。 | 相同。 | 不进入 staging。 |
| `computePairFeatures` 计算 `f1..f4`。 | 相同，仍在 RVV helper 内标量调用。 | 不采用 Phase 010 pair-feature RVV。 |
| Eigen `AngleAxisf` / `Affine3f` 计算 `alpha_m`。 | closed-form staging + RVV `atan2_RVV_f32m2`。 | 只替换 `alpha_m` 后段。 |
| 逐 pair 直接写 `PointOutT p`。 | 成功 pair 先 staging 后按 `output_rows` 写回 `alpha_m`。 | 写回顺序保持；staging 成本已由 production-public board 证据覆盖。 |

## Traceability Map

| 符号 / 文件 | 层级 | 作用 | 调用者 / 上游入口 | 被调用者 / 下游消费者 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- | --- | --- |
| `PPFEstimation::computeFeature` | production public entry | 初始化输出并执行 RVV / Std 分流。 | `PPFEstimation::compute()` | `computePPFFeatureAlphaMRVV` 或 `computePPFFeatureStd` | production boundary（生产边界） | `features/include/pcl/features/impl/ppf.hpp` |
| `computePPFFeatureStd` | production Std helper | 保留原标量 all-pairs、identity NaN、`computePairFeatures` 和 Eigen alpha 语义。 | public dispatch / fallback | `pcl::computePairFeatures`、Eigen transform | fallback source（回退来源） | `features/include/pcl/features/impl/ppf.hpp` |
| `computePPFFeatureAlphaMRVV` | production RVV helper | traits-gated source xyz AoS + normal AoS 下保留 `f1..f4` 标量计算，并批量 RVV 计算 `alpha_m`。 | `computeFeature` | `pcl::atan2_RVV_f32m2`、RVV intrinsic | adopted RVV path（已采用 RVV 路径） | `features/include/pcl/features/impl/ppf.hpp` |
| `test_ppf.cpp` | correctness tests | reference、candidate 和 production direct trace 对拍。 | `make run_test_compare` | topic-local helper | correctness gate（正确性验收） | `test-rvv/features/ppf/src/test_ppf.cpp` |
| `bench_ppf.cpp` | bench wrapper | 输出 diagnostic cases、Phase 040 exact public case 和 Phase 060 point-type expansion public case timing / checksum。 | `board_repeated` | manifest script | board performance input（板卡性能输入） | `test-rvv/features/ppf/src/bench_ppf.cpp` |
| `ppf_reference.hpp` | diagnostic reference | 复刻当前 production scalar 输出，用于 candidate 对拍。 | tests / bench | `computePairFeatures`、Eigen alpha reference | scalar oracle（标量判定参考） | `test-rvv/features/ppf/include/impl/ppf_reference.hpp` |
| `ppf_alpha_candidate.hpp` | candidate formula | 保存 Phase 020 closed-form 和 Phase 030 alpha batch RVV 诊断候选。 | tests / bench | `atan2_RVV_f32m2` | diagnostic source（诊断来源） | `test-rvv/features/ppf/include/impl/ppf_alpha_candidate.hpp` |
| `generate_ppf_evidence_manifest.py` | analysis script | 生成 production-public Evidence Doctor manifest。 | `make evidence_doctor_repeated` | `test-rvv/script/evidence_doctor.py` | doctor input（证据体检输入） | `test-rvv/features/ppf/script/generate_ppf_evidence_manifest.py` |
| Phase 040 result | phase result | 保存 PI1-PI5 生产接入、测试、asm、板卡和 PI5 决策。 | phase loop | 本文、evaluation、roadmap、matrix | production adoption source（生产采纳来源） | `test-rvv/features/ppf/doc/phases/040-production-alpha-m-rvv-integration/result.zh.md` |
| Phase 060 result | phase result | 保存 point-type expansion、fallback、asm、板卡 repeated 和停止判断。 | phase loop | 本文、evaluation、roadmap、matrix | production expansion source（生产扩展来源） | `test-rvv/features/ppf/doc/phases/060-point-type-expansion/result.zh.md` |
| PPF evaluation | topic-local evaluation | 保存函数级评估、候选取舍、生产接入判断和 Traceability Map。 | README / phase loop | 本文和 phase results | decision audit（决策审计） | `test-rvv/features/ppf/doc/ppf-evaluation.zh.md` |
| Board manifest / Doctor | evidence summary | 保存 Phase 040 exact case 与 Phase 060 point-type expansion 的 production-public board repeated 和 Evidence Doctor 结果。 | `board_repeated evidence_doctor_repeated` | evaluation / 本文 | summary-only evidence（仅摘要证据） | `test-rvv/features/ppf/log/board/repeated/evidence_manifest.json`、`test-rvv/features/ppf/log/board/phase060-pointxyzi-normal/repeated/evidence_manifest.json`、`test-rvv/features/ppf/log/board/phase060-pointxyz-pointnormal/repeated/evidence_manifest.json` |

## Bench 与证据

当前扩大覆盖的采纳决策只引用接入 production 后的 Phase 060 public case。命令：

```bash
make -C test-rvv/features/ppf REPEATED_BOARD_RUNS=5 REPEATED_BOARD_OUTPUT_DIR=log/board/phase060-pointxyzi-normal/repeated BENCH_ARGS="--side 28 --index-count 64 --repeat 8 --iterations 8 --warmup 2 --case-filter public_ppf_compute_pointxyzi_normal" board_repeated evidence_doctor_repeated
make -C test-rvv/features/ppf REPEATED_BOARD_RUNS=5 REPEATED_BOARD_OUTPUT_DIR=log/board/phase060-pointxyz-pointnormal/repeated BENCH_ARGS="--side 28 --index-count 64 --repeat 8 --iterations 8 --warmup 2 --case-filter public_ppf_compute_pointxyz_pointnormal" board_repeated evidence_doctor_repeated
```

### `PointXYZI + Normal -> PPFSignature`

| run | Std ms | RVV ms | speedup |
| --- | ---: | ---: | ---: |
| run_01 | 435.968 | 311.465 | 1.40x |
| run_02 | 439.786 | 306.714 | 1.43x |
| run_03 | 413.605 | 311.647 | 1.33x |
| run_04 | 440.339 | 311.700 | 1.41x |
| run_05 | 418.133 | 310.057 | 1.35x |
| mean | 429.5662 | 310.3166 | 约 1.384x |

### `PointXYZ + PointNormal -> PPFSignature`

| run | Std ms | RVV ms | speedup |
| --- | ---: | ---: | ---: |
| run_01 | 411.535 | 308.818 | 1.33x |
| run_02 | 411.882 | 310.287 | 1.33x |
| run_03 | 408.980 | 311.958 | 1.31x |
| run_04 | 412.259 | 309.951 | 1.33x |
| run_05 | 417.473 | 310.364 | 1.35x |
| mean | 412.4258 | 310.2756 | 约 1.33x |

两组 Evidence Doctor 结果均为 `Errors=0，Warnings=0，Suggestions=2`。QEMU bench smoke 只证明
`public_ppf_compute_pointxyzi_normal` 和 `public_ppf_compute_pointxyz_pointnormal` 可运行和日志形状；
性能结论只来自板卡 repeated summary。

## 正确性与高效性证据链

| 层级 | 当前证据 | 结论 | 不能外推的范围 |
| --- | --- | --- | --- |
| correctness（正确性） | `make -C test-rvv/features/ppf run_test_compare`；Std 5/5、RVV 9/9 pass；board RVV tests 9/9 pass。 | public RVV dispatch 命中 exact case 和两个代表性 traits-gated case，输出与 reference 在当前样本和误差预算内一致。 | 不证明其它 output type、`Scalar=double` 或所有非有限输入。 |
| production direct（生产直连） | `RVVAlphaMPathHitsPublicComputeForExactTypes`、`RVVAlphaMPathHitsPublicComputeForPointXYZILikeSource`、`RVVAlphaMPathHitsPublicComputeForPointNormalLikeNormals`。 | RVV 构建下 public `compute()` 真实进入 production RVV helper。 | trace 宏只在测试翻译单元启用，不进入 bench 证据。 |
| path / asm（路径 / 反汇编） | `dump_bench_rvv` 后三个 production helper 实例符号范围含 RVV load / store / float arithmetic 指令。 | 编译后的 RVV bench binary 包含目标手写 RVV 指令。 | 反汇编不单独证明性能。 |
| performance（性能） | Phase 060 production-public board repeated。 | source 和 normal 代表性扩展 public path 在目标板卡上稳定快于 scalar path。 | 不证明其它目标硬件、其它规模、其它 row source 或每个自定义 traits-compatible 点型。 |
| boundary（边界） | traits / AoS gate、fallback matrix、optimization matrix。 | 当前生产行为有明确范围，未覆盖路径保持标量。 | gate-allowed 自定义点型未逐个板卡覆盖。 |
| risk（风险） | Evidence Doctor `0E/0W/2S`。 | metadata suggestions 不阻塞采纳。 | 缺 taskset / governor / freq / temperature / binary hash 会降低严格归档可复核性。 |

## 生产接入后的 Closeout

| 项 | 最终状态 | 证据 |
| --- | --- | --- |
| 生产补丁范围 | adopted；只改 `features/include/pcl/features/impl/ppf.hpp`，未改变 public API。 | Phase 040 / 060 result、当前源码。 |
| 当前采用方式 | traits-gated source xyz AoS + normal AoS 下 `alpha_m` closed-form RVV batch；`f1..f4` 保持 `computePairFeatures` 标量 helper。 | production diff、`run_test_compare`、asm、board repeated。 |
| 覆盖范围 | source `RVVXYZAoSFloatLayout<PointInT>` + PPF normal AoS + exact `PPFSignature`、float AoS、ordered `indices_ x input_`。 | fallback matrix、production helper gate、Phase 060 representative board evidence。 |
| 不覆盖范围 | 自定义 traits-compatible 点型逐个板卡数据、`Scalar=double`、非 `PPFSignature` 输出、其它 row source、Phase 010 pair-feature RVV。 | optimization matrix、roadmap。 |
| production direct evidence | `PointXYZI + Normal` 5-run speedup `1.40, 1.43, 1.33, 1.41, 1.35`；`PointXYZ + PointNormal` 5-run speedup `1.33, 1.33, 1.31, 1.33, 1.35`。 | `test-rvv/features/ppf/log/board/phase060-pointxyzi-normal/repeated/evidence_manifest.json`、`test-rvv/features/ppf/log/board/phase060-pointxyz-pointnormal/repeated/evidence_manifest.json`。 |
| Evidence Doctor | Phase 060 两组 repeated report 均为 `0E/0W/2S`。 | `test-rvv/features/ppf/log/board/phase060-pointxyzi-normal/repeated/evidence_doctor.md`、`test-rvv/features/ppf/log/board/phase060-pointxyz-pointnormal/repeated/evidence_doctor.md`。 |
| 诊断到生产结论变化 | Phase 030 alpha diagnostic speedup 约 `1.57x` 只用于授权生产探针；最终采纳以 Phase 040 / 060 production-public 数据为准。 | Phase 030 / 040 / 060 result。 |
| 回退策略 | helper gate 不满足或非 RVV 构建时调用 `computePPFFeatureStd`。 | 源码 gate 与 Std/RVV tests。 |

## 后续方向

当前不建议在同一 PPF production boundary（生产边界）继续做微调。Phase 060 已关闭当前值得优先扩展的
point-type expansion；Phase 010 已证明 SoA-staged pair-feature batch RVV 在板卡上退化。继续优化
`f1..f4` 需要新的 direct-AoS 或 profile 证据，不能复用该负向候选。`alpha_m` 当前已经用
production-public 数据证明收益，继续压缩 staging buffer 或减少 `output_rows` 写回成本的预期收益较小，
且需要新增同边界 A/B、asm 和板卡证据。

若后续继续，建议拆成独立 phase 或 topic：

- evidence hardening（证据增强）：记录 taskset、governor、freq、temperature 和 binary hash，适合提交严格归档或出现长尾时执行。
- direct-AoS pair-feature revisit（点对特征直接结构数组重访）：只有 profile 证明 `f1..f4` 仍是主成本，且能避免 Phase 010 的 SoA staging 退化时再开。
- PPFRGB 跟随评估：复筛表中 PPFRGB 原先等待 PPF/CPPF 基础模式；现在 PPF 的 `alpha_m` 后段生产证据正向，但 PPFRGB 有 color / region search 语义，应作为单独 topic 重新评估。
