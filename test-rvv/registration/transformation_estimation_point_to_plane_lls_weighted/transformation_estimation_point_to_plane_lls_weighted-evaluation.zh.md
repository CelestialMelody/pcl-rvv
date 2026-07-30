# registration/transformation_estimation_point_to_plane_lls_weighted 函数级 RVV 评估

## 范围与结论

- 主题：`transformation_estimation_point_to_plane_lls_weighted`
- production 文件：`registration/include/pcl/registration/impl/transformation_estimation_point_to_plane_lls_weighted.hpp`
- 专项测试目录：`test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted/`
- 公开入口：`TransformationEstimationPointToPlaneLLSWeighted::estimateRigidTransformation`

当前 EvidenceDecision：

```text
production-candidate/full-cloud-f32-aos-layout-gated-weighted-block-dispatch-representative-pointtypes
```

生产候选只覆盖 full-cloud public overload（全云公开入口）、`Scalar=float`、连续 `weights_`、source `RVVXYZAoSFloatLayout`、target `RVVXYZNormalFloatLayout`、size/VLEN/byte-offset gate 均满足的路径。source-indexed（源索引路径）、dual-indices（双索引路径）、correspondences（对应关系路径）、`Scalar=double` 和非连续权重都保持标量。

主文档负责长期算法说明和设计理由：`doc-rvv/registration/transformation_estimation_point_to_plane_lls_weighted-RVV.zh.md`。本评估文档只记录测试矩阵、bench 边界、证据索引和决策审计。

## Production patch scope

| 项 | 当前状态 | 证据 |
| --- | --- | --- |
| dispatch | 只在 full-cloud overload 中尝试 RVV；命中后提前返回，否则进入原 iterator 标量 helper。 | production direct tests、源码审查。 |
| generic gate | source 用 `RVVXYZAoSFloatLayout`，target 用 `RVVXYZNormalFloatLayout`，两侧分别 offset/stride。 | `PointXYZ -> PointNormal`、`PointXYZ -> PointXYZINormal` tests。 |
| weights | 只读成员 `weights_` 连续 vector。 | public overload tests 和 production-dispatch bench 都调用 `setCorrespondenceWeights(weights)`。 |
| reduction | 采用 weighted full-cloud block-reduction；每个 chunk 用 `vfmacc` 更新 A/B/C/N partial sums，每组扫完 block 后显式 `vfredosum`。 | normal-equation tests、asm attribution、board 5-run。 |
| fallback | 非 RVV、非 float、小规模、layout miss、VL miss、byte-offset miss、indexed/correspondences 均标量。 | fallback tests 和源码审查。 |

## 标量流程与 RVV 流程对照

| 阶段 | 标量路径 | 当前 RVV production |
| --- | --- | --- |
| 入口检查 | full-cloud overload 检查 source/target 点数和 `weights_.size()`。 | 保留原检查。 |
| row source | `ConstCloudIterator` 同步读取 `source[k]` 和 `target[k]`。 | 只接 full-cloud，source/target 直接按 AoS stride load。 |
| weight | `weights_it` 顺序读取。 | `vle32.v` 连续加载 `weights_[k]`。 |
| finite mask | 检查 source xyz、target xyz 和 target normal，不检查 weight。 | 同语义，invalid lane merge to zero。 |
| formula | `normal *= weight` 后计算 `a/b/c/d`。 | 同公式，当前不使用 `a/b/c/d` 逐点公式树 fused contraction。 |
| accumulation | 按 row 顺序用 double 累加 `ATA/ATb`。 | 每个 block 用 A/B/C/N vector partial sums；chunk 内用 `vfmacc` 累加，组结束后再 `vfredosum` 到 double normal-equation。 |
| solve / matrix | Eigen 6x6 inverse solve，构造 4x4 matrix。 | 保留标量。 |

## 函数族评估表

| 路径 | 当前决策 | 证据边界 |
| --- | --- | --- |
| full-cloud current diagnostic | 保留为 baseline | `vcompress + fixed buffer + tail` 可对拍，但不是 production 默认。 |
| full-cloud block-reduction | production candidate | 已有 production direct/fallback、numeric stress、asm 和 representative board 5-run。 |
| source-indexed | 仅保留诊断证据 | correctness 已建，board single-run 弱正向，缺 repeated board 和 production direct/fallback。 |
| dual-indices | 仅保留诊断证据 | correctness 已建，board single-run 负向。 |
| correspondences | 仅保留诊断证据 | correctness 已建，board single-run 负向，且混入 index/weight 展开和 gather 多个成本源。 |
| fused formula | deferred | 当前 block-reduction 已用 `vfmacc` 做 partial-sum accumulation；缺的是 `a/b/c/d` 逐点公式树 fused contraction 的 weighted same-chain、near-cancellation、非有限 weight、asm 和 board A/B。 |
| `Scalar=double` | fallback | 有 fallback smoke；不进入 RVV。 |

## RowSourcePolicy + WeightPolicy 测试矩阵

| 入口形态 | RowSourcePolicy 取点 | WeightPolicy 取权重 | 关键测试 | Bench case | 当前判断 |
| --- | --- | --- | --- | --- | --- |
| full-cloud | `source[k] + target[k]` | `weights_[k]` 连续读取 | `StdDiagnosticMatchesPublicEstimator`、`FullCloudCandidateMatchesStd` | `weighted lls full-cloud pointnormal` | diagnostic baseline。 |
| full-cloud block-reduction | 同 full-cloud | `weights_[k]` 连续读取 | `FullCloudBlockReductionMatchesStdWithinBudget`、production direct/fallback tests | `weighted lls full-cloud block-reduction pointnormal`、production-dispatch representative rows | production candidate。 |
| source-indexed | `source[indices_src[k]] + target[k]` | `weights_[k]` 连续读取 | `StdSourceIndexedMatchesPublicEstimator`、`SourceIndexedCandidateMatchesStd` | `weighted lls source-indices pointnormal` | 仅保留诊断证据。 |
| dual-indices | `source[indices_src[k]] + target[indices_tgt[k]]` | `weights_[k]` 连续读取 | `StdDualIndicesMatchesPublicEstimator`、`DualIndicesCandidateMatchesStd` | `weighted lls dual-indices pointnormal` | 仅保留诊断证据。 |
| correspondences | 展开 `index_query/index_match` 后 gather | 展开 `correspondence.weight` | `StdCorrespondencesMatchesPublicEstimator`、`CorrespondenceCandidateMatchesStd` | `weighted lls correspondences pointnormal` | 仅保留诊断证据。 |

Weight finite semantics（权重有限性语义）由 `NonFiniteWeightsAreNotMaskedWhenPointsAreFinite` 和 `ProductionFullCloudPreservesNonFiniteWeightSemantics` 保护：point/normal 非有限会被剔除，weight 非有限但 point/normal 有限时仍参与计算。

## Production direct 与 fallback 测试

| 测试 | 层级 | 覆盖内容 |
| --- | --- | --- |
| `ProductionFullCloudPublicOverloadMatchesStdWithinBudget` | production direct | 真实 public full-cloud overload 的 matrix 对拍。 |
| `ProductionFullCloudNormalEquationMatchesStdWithinBudget` | production direct / normal-equation | `accepted_points`、`ATA/ATb` 和 matrix 预算。 |
| `ProductionFullCloudScaleStressMatchesStdWithinBudget` | numeric stress | 大尺度输入和权重动态范围下的 matrix / normal-equation。 |
| `ProductionFullCloudPreservesNonFiniteWeightSemantics` | numerical consistency | point/normal finite mask 与非有限 weight 语义。 |
| `ProductionFullCloudSmallInputFallsBackToScalar` | fallback | `n < 64` 回标量。 |
| `ProductionFullCloudPredicateGatesAreNarrow` | gate | size、weights size、VL 和 byte-offset predicate。 |
| `ProductionFullCloudPointXYZSourceMatchesStdWithinBudget` | generic source | `PointXYZ -> PointNormal` 命中 source xyz f32 AoS gate。 |
| `ProductionFullCloudPointXYZToPointXYZINormalMatchesStdWithinBudget` | generic target | `PointXYZ -> PointXYZINormal` 命中 target xyz+normal f32 AoS gate。 |
| `ProductionFullCloudDoubleNormalTargetFallsBackToScalar` | layout fallback | target normal 不是单个 float 字段时回标量。 |
| `ProductionFullCloudScalarDoubleFallsBackToScalar` | scalar fallback | `Scalar=double` 回标量。 |

完整专项矩阵还保留 diagnostic tests：四条 row source 对拍、小规模 isolated gate、invalid lane mask、非有限 weight 语义和 block-reduction A/B。保留这些测试是为了把 row source、weight source、mask、reduction tree 和 production dispatch 分层审查。

## Bench 边界

输入在计时前构造：source 是确定性曲面，target 由 `transformPointCloudWithNormals` 生成，full/source/dual 权重是周期序列，indices 和 correspondences 是确定性有效输入。

测量包含 normal-equation 构造、Eigen solve 和 matrix 构造。full-cloud 不包含权重生成；source-indexed 和 dual-indices 的 `pcl::Indices` 输入在计时前构造，但 candidate 内部 valid-index scan、`uint32_t` staging、byte-offset prepare、index load/gather 和连续 weight load 计入；correspondences case 包含 candidate 内部 index/weight 展开。

| case | 入口 | 证明点 | 不能证明什么 |
| --- | --- | --- | --- |
| `weighted lls full-cloud pointnormal` | 测试专用 `estimate_candidate_full` | full-cloud current baseline 的 stride load、contiguous weight、mask/staging。 | production dispatch、generic 点型、indexed/correspondences。 |
| `weighted lls full-cloud block-reduction pointnormal` | 测试专用 block helper | A/B/C/N partial sums 和显式 `vfredosum`。 | production dispatch；生产性能由 production-dispatch 5-run 证明。 |
| `weighted lls source-indices pointnormal` | 测试专用 source-indexed helper | 单侧 source gather + target stride + continuous weight。 | dual gather、correspondence weight 展开、生产收益。 |
| `weighted lls dual-indices pointnormal` | 测试专用 dual helper | 双侧 gather 与连续 weight stream。 | correspondence parsing 和生产收益。 |
| `weighted lls correspondences pointnormal` | 测试专用 correspondences helper | index/weight 展开 + gather 语义。 | 单因归因到 gather、任意真实 correspondence 分布。 |
| `weighted lls production-dispatch full-cloud pointnormal` | 真实 public overload | `PointNormal -> PointNormal` 子集的 public dispatch 性能。 | 所有泛型点型、indexed/correspondences。 |
| `weighted lls production-dispatch full-cloud pointxyz-to-pointnormal` | 真实 public overload | generic source representative 性能。 | 所有 source 点型。 |
| `weighted lls production-dispatch full-cloud pointxyz-to-pointxyzinormal` | 真实 public overload | generic target representative 性能。 | 所有 target normal 点型。 |

## 证据索引与结果

| 证据 | 路径 / 命令 | 结果 |
| --- | --- | --- |
| QEMU tests | `make -C test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted run_test_compare` | std/RVV 各 25 tests passed。 |
| board tests | `output/board/run_test.log` | board 25 tests passed。 |
| QEMU bench shape | `make -C ... run_bench_compare` 和 production-dispatch case-filter | 可解析，checksum 基本对齐；QEMU timing 不作性能结论。 |
| asm | `make -C ... dump_bench_rvv` | production helper 独立符号内确认 stride load、contiguous weight load、A/B/C/N partial sums、`vcpop/vmerge` 和显式 `vfredosum`。 |
| 10-case board diagnostic | 文档 summary；顶层 log 会被 case-filter 覆盖 | single-run diagnostic signal，不是 production performance evidence。 |
| representative board 5-run | `output/board/production_dispatch_weighted_generic_representative_5run_summary.md` | 三类代表点型 64K/256K median 均正向。 |

production-dispatch representative 5-run 摘要：

| case | runs | 64K median/min | 256K median/min | 说明 |
| --- | ---: | ---: | ---: | --- |
| `pointnormal` | 5 | `2.69x / 2.66x` | `2.71x / 2.11x` | `PointNormal -> PointNormal` 子集；256K 有一轮低谷但仍正向。 |
| `pointxyz-to-pointnormal` | 5 | `2.81x / 2.79x` | `2.83x / 2.69x` | source generic gate representative。 |
| `pointxyz-to-pointxyzinormal` | 5 | `2.81x / 2.80x` | `2.85x / 2.83x` | source + target generic gate representative。 |

QEMU 10-case diagnostic table 和 board 10-case diagnostic table 只用于日志形状和诊断信号。当前性能结论只使用 representative 5-run board summary。

## 反汇编归属

`buildPointToPlaneLLSWeightedFullCloudBlockRVV` 是当前 production helper 的关键符号。该符号范围内确认：

- `vlse32.v`：source xyz、target xyz 和 target normal 的 AoS stride load。
- `vle32.v`：连续 `weights_` load。
- `vcpop.m` / `vmerge`：finite mask 和 invalid lane 归零。
- `vfmacc.vv`：每个 chunk 更新 A/B/C/N partial sums。
- `vfredosum.vs`：每个 A/B/C/N group 扫完 block 后，对该组 partial sums 做显式横向规约。

全二进制中的其它 `vfmadd/vfmacc` 或 `vfred*` 可能来自 Eigen、bench harness 或自动向量化，不能归因到当前 helper。当前 production 行为已经包含 normal-equation partial-sum `vfmacc`；fused formula deferred 指的是 `a/b/c/d` 逐点公式树 contraction，不是禁止或缺失所有 FMA 指令。

## 正确性与高效性证据链

| 维度 | 证据 | 结论 | 边界 |
| --- | --- | --- | --- |
| correctness | 25 tests QEMU + board；production direct/fallback；numeric stress；非有限语义。 | `accepted_points`、`ATA/ATb`、matrix 和 fallback 均有覆盖。 | 不覆盖非法 index、所有 correspondence 分布、`Scalar=double` RVV。 |
| path/asm | production helper 符号内指令归属。 | RVV 热点确实来自 weighted block-reduction helper。 | 不把全二进制 FMA/reduction 当作当前公式证据。 |
| performance | representative production-dispatch board 5-run。 | 支持 full-cloud f32 AoS layout-gated weighted block dispatch。 | 只覆盖三类代表点型，不证明所有 gate-allowed 点型逐类型性能。 |
| boundary | RowSourcePolicy / WeightPolicy 矩阵和 fallback tests。 | source/dual/correspondences、`Scalar=double`、非连续权重保持标量。 | diagnostic 10-case 不能替代 production evidence。 |

## EvidenceDecision 审计

支持当前 production candidate 的条件：

- full-cloud public overload 已有真实 direct tests。
- size、VL、layout、byte-offset、`Scalar` 和 small-input fallback 均有测试或源码审查。
- production helper 独立符号内完成 asm attribution。
- board gtest 是 25 tests。
- representative production-dispatch 5-run board summary 在三类点型、64K/256K 上正向。

证据不支持的扩展：

- source-indexed production：缺 repeated board、production direct/fallback 和符号级生产归属。
- dual-indices production：single-run board diagnostic 负向，缺 production direct/fallback。
- correspondences production：single-run board diagnostic 负向，且成本源包含 index/weight 展开、gather、压缩和 tail；缺消融。
- fused formula：当前 block-reduction 已有 partial-sum `vfmacc`；后续只评估 `a/b/c/d` 逐点公式树 contraction。缺 weighted same-chain、非有限 weight、near-cancellation、asm 和 board A/B。
- `Scalar=double` 和未逐类型上板的点型性能。

因此 current EvidenceDecision 保持为 `production-candidate/full-cloud-f32-aos-layout-gated-weighted-block-dispatch-representative-pointtypes`。

## 遗留风险

- 代表点型不是逐类型证明。新增 gate-allowed 点型的性能结论需要对应 board evidence。
- `PointNormal 262144` representative row 有一轮 `2.11x` 低谷，仍为正向，但不应写成零波动稳定性。
- source/dual/correspondences 的负向归因尚未消融，不能写成“唯一主因是 gather”。
- current diagnostic 的 `vcompress + buffer + tail` 仍可作为消融 baseline，但 production 默认已采用 block-reduction。

## Fused formula follow-up design

unweighted TEPTPL 的 fused formula 已经迁移到 production：它保留 A/B/C/N block-reduction，逐点 `a/b/c` 使用 `vfmsac`，`d` 使用 `nx*(dx-sx) + ny*(dy-sy) + nz*(dz-sz)` 后用 `vfmacc` 累加。weighted TEPTPLW 不能直接采用同一结论，因为 weighted 先执行 `normal *= weight`，非有限 weight 不参与 finite mask，并且 current production evidence 只批准 block-reduction partial sums。

建议下一轮只做 test_support diagnostic/component A/B，不先改 production：

| 候选 | 变化 | 测试预算 | bench 角色 |
| --- | --- | --- | --- |
| `abc-fused` | `a/b/c` 使用 `vfmsac`，`d` 保持当前展开。 | weighted same-chain、scale-stress、`ATA/ATb`、matrix。 | isolate `a/b/c` 公式树成本。 |
| `d-six-term-fma` | 保留 `d` 六项形态，但用 FMA 累加。 | 非有限 weight、near-cancellation、`accepted_points`、`ATA/ATb`。 | isolate `d` 六项舍入树。 |
| `d-displacement-fused` | 使用 `dx-sx`、`dy-sy`、`dz-sz` 后 `vfmacc`。 | 非有限 weight 传播、near-cancellation、scale-stress、matrix。 | 对照 unweighted 同款 `d` 公式树。 |
| `abcd-fused` | 组合 `abc-fused` 和通过预算的 `d` candidate。 | 上述预算全量覆盖。 | 只在 isolated candidates 通过后做组合 A/B。 |

测试需要显式检查非有限 weight 语义、near-cancellation、scale-stress、`accepted_points`、`ATA/ATb` 和 matrix。非有限 point/normal 仍由 finite mask 排除；非有限 weight 不能改变 `accepted_points`，但会继续影响 normal-equation，这一点必须和标量 reference 对齐。

test_support 设计建议在 `teptplw_reductions.hpp` 增加 fused formula lane helper，在 `teptplw_candidates.hpp` 增加 direct diagnostic candidate。bench 先加 component no-solve 和 normal-equation direct cases，例如：

```text
weighted lls component full-cloud block-fused-abc no-solve pointnormal
weighted lls component full-cloud block-fused-d-six-term no-solve pointnormal
weighted lls component full-cloud block-fused-d-displacement no-solve pointnormal
weighted lls normal-equation full-cloud block-fused-abcd pointnormal
```

asm gate 需要在 fused diagnostic helper 符号范围内区分两类 FMA：逐点公式树里的 `vfmsac/vfmacc`，以及当前 already-adopted A/B/C/N partial-sum `vfmacc`。`vfredosum` 仍应只归属于每组结束后的横向规约。板卡性能需要 representative 5-run A/B summary-only artifact；第一轮建议只做 `PointNormal -> PointNormal` 64K/256K，若稳定正向且 correctness/numeric 预算闭合，再扩到 `PointXYZ -> PointNormal` 和 `PointXYZ -> PointXYZINormal`。QEMU timing 不作为性能结论。

建议先提交当前 block candidate，再另开 fused formula worker。当前 block-reduction production candidate 的 correctness、fallback、asm 和 representative board 证据已经闭合；fused formula 会改变逐点舍入树和非有限 weight 传播，适合独立 review 和独立 evidence artifact。
