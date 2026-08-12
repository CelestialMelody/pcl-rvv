# registration/transformation_estimation_point_to_plane_lls_weighted 函数级 RVV 评估

## 范围与结论

- 主题：`transformation_estimation_point_to_plane_lls_weighted`
- production 文件：`registration/include/pcl/registration/impl/transformation_estimation_point_to_plane_lls_weighted.hpp`
- 专项测试目录：`test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted/`
- 公开入口：`TransformationEstimationPointToPlaneLLSWeighted::estimateRigidTransformation`

EvidenceDecision：

```text
bounded-production-candidate/full-cloud-adopted-and-source-indexed-block-fused-probe-f32-aos-valid-index-positive-with-warnings
```

生产接入范围是 full-cloud public overload 和 source-indexed public overload。两条路径都要求 `Scalar=float`、连续 `weights_`、source `RVVXYZAoSFloatLayout`、target `RVVXYZNormalFloatLayout`、size/VLEN/byte-offset gate 均满足。full-cloud 路径保留 weighted block-reduction，并在逐点 `a/b/c/d` 公式块中采用 `fused-abcd-ilp` code shape。source-indexed 路径在 Phase 031 后先尝试 `block-fused-abcd-ilp` production probe（生产探针）helper；该 helper 会先完成 valid-index scan 和 `uint32_t` staging，再用 source gather、target stride load、A/B/C/N block groups 构造 normal-equation。probe helper 返回 false 时回到 prior staged-gather / compressed-tail helper，再失败时回到标量路径。

dual-indices、correspondences、`Scalar=double`、非连续权重和 layout miss 路径保持标量。

## 文档分工

本评估文档只记录决策审计、证据索引和剩余风险。细节放在下列文档中：

| 问题 | 文档 |
| --- | --- |
| 测试类型、运行入口、覆盖矩阵和证据边界 | `testing-overview.zh.md` |
| 每个 gtest 的中文含义、输入、断言和代码位置 | `correctness-tests.zh.md` |
| bench label、case-filter、checksum、trace、asm 和提交边界 | `benchmark-and-evidence.zh.md` |
| 每种 RVV 优化方式的代码路径、target、结果和边界 | `optimization-evidence.zh.md` |
| `include/impl` 与 `src` 的函数族、调用关系和 production/test 边界 | `test-support-code-map.zh.md` |
| production 实现长期说明 | `../../../../doc-rvv/registration/transformation_estimation_point_to_plane_lls_weighted-RVV.zh.md` |

## Production Patch Scope

| 项 | 当前状态 | 证据 |
| --- | --- | --- |
| dispatch | full-cloud overload 先检查 source/target 点数和 `weights_.size()`，再尝试 RVV。命中后提前返回，失败后进入原 iterator 标量 helper。 | 真实生产路径测试、公开入口输入语义测试、源码审查。 |
| source-indexed dispatch | source-indexed overload 先检查 index count、target 点数和 `weights_.size()`，再尝试 block-fused probe；probe helper 失败后尝试 staged-gather helper，仍失败才进入原 iterator 标量 helper。 | `run_test_source_indices_compare`、`run_test_production_direct_compare`、source-indexed probe board summary、源码审查。 |
| generic gate | source 用 `RVVXYZAoSFloatLayout`，target 用 `RVVXYZNormalFloatLayout`。两侧分别取 offset 和 stride。 | 三类代表点型 correctness、production-dispatch board summary。 |
| weights | 只读取成员 `weights_` 的连续 vector。 | public overload tests 和 production-dispatch bench 都调用 `setCorrespondenceWeights(weights)`。 |
| finite mask | 检查 source xyz、target xyz 和 target normal。weight 不参与 finite mask。 | 非有限 point/normal tests、非有限 weight tests。 |
| reduction | 使用 A/B/C/N block groups。每组在向量 partial sums 中累加，再显式 `vfredosum` 到 double normal-equation。 | 法方程测试、反汇编归因、板卡追踪。 |
| formula | `a/b/c` 使用 seed multiply + `vfmsac`；`d` 使用 displacement 后 `vfmul + vfmacc + vfmacc`。 | production source、反汇编归因、checksum trace。 |
| fallback | 非 RVV、非 float、小规模、layout miss、VL miss、byte-offset miss、source-index invalid、dual-indices 和 correspondences 均回到标量边界。 | 回退路径测试和源码审查。 |
| source-indexed family | Phase 031 后当前 production source-indexed 先走 block-fused-abcd-ilp bounded candidate；staged-gather / compressed-tail 作为 rollback helper 和 prior production baseline 保留。 | `production_source_indices_block_fused_abcd_ilp_probe/summary.md` 显示 6 个代表 case median 均正向；`production_source_indices_staged_gather/summary.md` 保留旧基线。 |
| family carry-over audit | full-cloud adopted 的 block-reduction / A/B/C/N / fused-abcd-ilp family 已按 source-indexed、dual-indices、correspondences 启动 test-rvv 审计；source-indexed-family 的 Phase 030 diagnostic negative 被 Phase 031 production direct probe 校准为 historical diagnostic / harness-risk signal。 | Phase 030：source-indexed `block-fused-abcd-ilp` full estimate median `0.90x` 且 `4/5` 低于 `1.0x`；Phase 031：真实 public dispatch probe median `1.54x` 到 `1.71x`，Doctor `0E / 9W / 12S`。 |

## Evidence Index

| 证据 | 路径 / 命令 | 结果 | 边界 |
| --- | --- | --- | --- |
| QEMU 正确性 | `log/qemu/run_test_std.log`、`log/qemu/run_test_rvv.log` | std 为 `45 passed + 1 skipped`；RVV 为 `47 passed`。 | 正确性、路径和日志形状；不作为性能结论。 |
| source-indexed 细粒度正确性 | `log/qemu/run_test_source_indices_std.log`、`log/qemu/run_test_source_indices_rvv.log` | std 6 passed；RVV 7 passed。 | 真实 public source-indexed overload、default helper、fallback 和代表点型。 |
| row-source 诊断触发 | `log/board/run_board_bench_row_sources/analyze_bench_compare.log`、`log/board/run_board_bench_row_sources/evidence_manifest.json`、`log/board/run_board_bench_row_sources/evidence_doctor.md` | source-indexed 65536 / 262144 正向；dual-indices 和 correspondences 负向；doctor 为 0 Errors / 0 Warnings / 0 Suggestions。 | 只说明 pre-production diagnostic 边界成立；不作为最终 production 性能结论。 |
| production-dispatch 重复板卡采集 | `log/board/production_dispatch_fused_abcd_ilp/summary.md` | 三类代表点型 262144 点 repeated std/RVV speedup 均正向。 | 真实 public full-cloud overload；不覆盖 source-indexed / dual / correspondences。 |
| production-dispatch Evidence Doctor | `log/board/production_dispatch_fused_abcd_ilp/evidence_manifest.json`、`log/board/production_dispatch_fused_abcd_ilp/evidence_doctor.md` | 0 Errors / 0 Warnings / 3 Suggestions；binary identity suggestion 不阻塞当前结论。 | 只覆盖 production-dispatch full-cloud summary 和补充的默认 path checksum / asm。 |
| source-indexed prior production 重复板卡采集 | `log/board/production_source_indices_staged_gather/summary.md` | 三类代表点型在 65536 和 262144 点 repeated std/RVV speedup 均正向。 | Phase 031 前的 staged-gather / compressed-tail production baseline；现在作为 rollback / historical production evidence。 |
| source-indexed block-fused production probe | `log/board/production_source_indices_block_fused_abcd_ilp_probe/summary.md` | 6 个代表 case median 均正向：`1.54x` 到 `1.71x`。 | 真实 public source-indexed overload；支持 bounded production candidate，不覆盖 dual-indices、correspondences 或 invalid index public 行为。 |
| source-indexed probe Evidence Doctor 边界 | `log/board/production_source_indices_block_fused_abcd_ilp_probe/evidence_manifest.json`、`log/board/production_source_indices_block_fused_abcd_ilp_probe/evidence_doctor.md` | 0 Errors / 9 Warnings / 12 Suggestions。 | 无阻塞 Error，但 source-indexed-specific asm、binary identity、taskset metadata 和 262144 长尾仍未闭合，不能写成 clean pass。 |
| source-indexed family diagnostic | `log/board/run_board_bench_source_indexed_family/evidence_manifest.json`、`log/board/run_board_bench_source_indexed_family/evidence_doctor.md`、`log/board/source_indexed_family_repeated/summary.md`、`log/board/source_indexed_family_repeated/evidence_doctor.md` | Phase 030 repeated summary：staged median `1.04x`、block-baseline median `1.05x`、block-fused median `0.90x`；doctor 为 `3E / 6W / 13S`。 | 现在作为 historical diagnostic / harness-risk signal；Phase 031 production direct probe 是当前生产探针事实。 |
| production-default 追踪 | `log/board/production_default_fused_abcd_ilp/trace_summary.md` | 5 runs、20 iterations、5 warm-up；三类点型 checksum 序列一致。 | 默认 RVV path 稳定性；不产出旧 block/fused B/A。 |
| 校验和稳定性 | `log/board/production_default_fused_abcd_ilp/checksum_validation.md` | 5 轮 checksum 行齐全，序列一致。 | 日志指纹一致性；不替代 gtest 数值正确性。 |
| 生产反汇编归因 | `log/board/production_default_fused_abcd_ilp/asm_production_symbol_attribution.md` | 默认 production helper 的 RVV 指令归属已记录 `boundary`。 | 不同 `boundary` 的总指令数不能直接横向比较。 |

本轮没有重跑 board gtest。当前可提交 board 证据是重复性能摘要、追踪摘要、校验和稳定性和反汇编归因。

## Production-Dispatch Result

production-dispatch repeated board 使用 262144 点、5 runs、20 iterations 和 5 warm-up iterations。speedup 公式是：

```text
std/RVV speedup = std_ms / rvv_ms
```

| case | 中文含义 | median/min | 结论边界 |
| --- | --- | ---: | --- |
| `weighted lls production-dispatch full-cloud pointnormal 262144` | `PointNormal -> PointNormal` 真实 public overload。 | `2.76x / 2.73x` | 代表 `PointNormal` 布局。 |
| `weighted lls production-dispatch full-cloud pointxyz-to-pointnormal 262144` | source generic gate representative。 | `2.98x / 2.95x` | 代表 source 只提供 xyz。 |
| `weighted lls production-dispatch full-cloud pointxyz-to-pointxyzinormal 262144` | source + target generic gate representative。 | `3.00x / 2.96x` | 代表 target 有 xyz+normal 和额外字段。 |

这些结果证明 full-cloud production default 在三类代表点型上相对 std 正向。它们不证明 source-indexed、dual-indices、correspondences、`Scalar=double`、非连续权重或所有 gate-allowed 点型逐类型性能。source-indexed 结论见下一节。

## Source-Indexed Production Probe Result

Phase 031 后，source-indexed production repeated board 使用独立 probe 目录：

```text
log/board/production_source_indices_block_fused_abcd_ilp_probe/summary.md
```

它仍使用 `production-source-indices` case-filter、65536/262144 点、5 runs、20 iterations 和 5 warm-up iterations。speedup 公式同样是：

```text
std/RVV speedup = std_ms / rvv_ms
```

| case | 中文含义 | median/min | 结论边界 |
| --- | --- | ---: | --- |
| `weighted lls production-dispatch source-indices pointnormal 262144` | `PointNormal -> PointNormal` 真实 source-indexed public overload。 | `1.64x / 1.17x` | 代表 `PointNormal` source gather；262144 下有长尾 warning。 |
| `weighted lls production-dispatch source-indices pointnormal 65536` | 同上，较小规模。 | `1.71x / 1.66x` | 代表 64K index stream。 |
| `weighted lls production-dispatch source-indices pointxyz-to-pointnormal 262144` | source 是 `PointXYZ`，target 是 `PointNormal`。 | `1.54x / 1.06x` | 代表 source generic xyz layout；262144 下有长尾 warning。 |
| `weighted lls production-dispatch source-indices pointxyz-to-pointnormal 65536` | 同上，较小规模。 | `1.64x / 1.59x` | 代表 64K generic source。 |
| `weighted lls production-dispatch source-indices pointxyz-to-pointxyzinormal 262144` | source 是 `PointXYZ`，target 是 `PointXYZINormal`。 | `1.69x / 1.41x` | 代表 source 和 target generic layout；262144 下有长尾 warning。 |
| `weighted lls production-dispatch source-indices pointxyz-to-pointxyzinormal 65536` | 同上，较小规模。 | `1.69x / 1.68x` | 代表 64K generic source + target。 |

这些结果证明 valid source-indexed public dispatch 接入 block-fused probe 后，在三类代表点型和两个规模上相对 std 正向。它们不证明 dual-indices、correspondences、invalid index public API 行为、`Scalar=double` 或非连续权重。因为 Doctor 仍有 asm boundary、binary identity 和长尾 warning，当前结论是 bounded production candidate，不是 clean adopted。

Phase 031 前的 staged-gather / compressed-tail summary 保留为旧基线和 rollback evidence：

| case | 中文含义 | median/min |
| --- | --- | ---: |
| `weighted lls production-dispatch source-indices pointnormal 262144` | `PointNormal -> PointNormal`。 | `2.33x / 1.99x` |
| `weighted lls production-dispatch source-indices pointnormal 65536` | 同上，较小规模。 | `2.59x / 2.47x` |
| `weighted lls production-dispatch source-indices pointxyz-to-pointnormal 262144` | source 是 `PointXYZ`，target 是 `PointNormal`。 | `2.22x / 2.18x` |
| `weighted lls production-dispatch source-indices pointxyz-to-pointnormal 65536` | 同上，较小规模。 | `2.57x / 2.49x` |
| `weighted lls production-dispatch source-indices pointxyz-to-pointxyzinormal 262144` | source 是 `PointXYZ`，target 是 `PointXYZINormal`。 | `2.37x / 2.28x` |
| `weighted lls production-dispatch source-indices pointxyz-to-pointxyzinormal 65536` | 同上，较小规模。 | `2.78x / 2.70x` |

## Correctness Boundary

QEMU `run_test_compare` 覆盖下列 correctness 维度：

| 维度 | 覆盖状态 |
| --- | --- |
| 公开入口语义测试 | full-cloud、source-indexed、dual-indices 和 correspondences 的有效输入语义。 |
| 公开入口输入语义测试 | full/source/dual 的 target、index stream 和 weights 数量不匹配；0 权重；负权重。 |
| 真实生产路径测试 | 真实 full-cloud public overload、source-indexed public overload、default helper、std helper 和 production normal-equation。 |
| 回退路径测试 | small input、predicate gate、double-normal target、`Scalar=double`。 |
| 数值一致性测试 | near-cancellation、scale-stress、非有限 point/normal、非有限 weight。 |
| 代表点型覆盖 | `PointNormal -> PointNormal`、`PointXYZ -> PointNormal`、`PointXYZ -> PointXYZINormal`。 |

暂缓项：

| 项 | 原因 |
| --- | --- |
| empty input | 需要先确认 upstream 对奇异 solve 和输出矩阵的期望。 |
| invalid indices / invalid correspondences | `ConstCloudIterator` 使用 index 访问；test-only reference 的 defensive skip 不能外推成 public API 合同。 |
| `Scalar=double` 严格 reference 对拍 | 当前已有 fallback smoke。更强 reference 需要先定义 double 误差预算。 |

## Row Source Decision Boundary

数据源取舍需要分两层复核。第一层是 diagnostic candidate，检查不同 row source 的正确性和耗时形状。第二层是 production direct，检查真实 public overload 是否接入、是否有 fallback、反汇编归属和 repeated board 性能。

当前新增 `run_bench_row_sources` 作为第一层的专门入口。对应的单次板卡入口是 `run_board_bench_row_sources`，重复板卡采集入口是 `collect_board_row_sources_repeated`。`run_bench_default_diagnostic` 仍是综合诊断入口，它包含 row source case，但同时混入 full-cloud、block-reduction 和 fused formula case。

`run_board_bench_row_sources/analyze_bench_compare.log` 记录了 source-indexed 65536 / 262144 正向、dual-indices 和 correspondences 负向的单次板卡信号。对应的 `run_board_bench_row_sources/evidence_manifest.json` 把这些值保留为 `observed_speedup`，doctor 结果为 0 Errors / 0 Warnings / 0 Suggestions。它是 production integration 的触发 / 拒绝诊断证据，不是最终 production 性能结论；source-indexed 的最终结论由 `collect_board_production_source_indices_repeated` 收口。

source-indexed 先走完 staged-gather / compressed-tail adoption 链路，随后在 Phase 031 按用户授权做了 block-fused production direct probe。Phase 031 说明 test-rvv family diagnostic 的负向不能直接预测真实 production dispatch；若后续继续升级 source-indexed block-fused，需要补 source-indexed-specific asm、binary identity、taskset metadata 和可选 extended-run。dual-indices 和 correspondences 则还停在 diagnostic candidate 阶段，必须先补自己的 candidate / test / bench / board，再谈 production。

| 入口形态 | correctness target | bench target | board target | 当前状态 | 生产接入前还缺什么 |
| --- | --- | --- | --- | --- | --- |
| full-cloud | `run_test_public_semantics`、`run_test_production_direct` | `run_bench_production_dispatch` | `collect_board_production_dispatch_repeated` | 已采纳 | 已有当前真实生产路径证据。 |
| source-indexed | `run_test_public_semantics`、`run_test_source_indices`、`run_test_production_direct` | `run_bench_production_source_indices` | `collect_board_production_source_indices_probe_repeated` | bounded production candidate | 已有 source-indexed production direct、fallback、bench 和 probe repeated board 证据；clean adopted 仍缺 source-indexed-specific asm / binary identity / taskset metadata / extended-run。 |
| dual-indices | `run_test_public_semantics`、`run_test_row_sources` | `run_bench_row_sources` | `collect_board_row_sources_repeated` | 仅诊断 | production helper、public dispatch、回退路径测试、production asm、repeated board。 |
| correspondences | `run_test_public_semantics`、`run_test_row_sources` | `run_bench_row_sources` | `collect_board_row_sources_repeated` | 仅诊断 | production helper、public dispatch、回退路径测试、production asm、repeated board；还要拆 index/weight 展开成本。 |

如果 `run_bench_row_sources` 或对应 board 诊断显示某个 row source 稳定正向，只能把该方向升级为下一轮 production integration candidate。接入 production 之前，不能把该诊断结果写入当前 EvidenceDecision。source-indexed 已经走完这条升级链路，因此不再停留在 candidate。

## Historical Decision Audit

fused formula follow-up 先在 test-rvv diagnostic 层筛选公式树候选，再补 production-symbol 和默认 production 证据。当前 production selector 已采用 `abcd-fused-ilp` 公式块。其它 `abc`、D 项和非 ILP 变体仍作为 test_support 消融候选。

历史 fused-vs-block B/A 使用：

```text
B/A = block-baseline_rvv_ms / fused-candidate_rvv_ms
```

`B/A > 1` 表示 fused candidate 在同一边界下更快。历史数据只能解释候选取舍。当前生产性能结论使用 production-dispatch repeated summary。

| 历史阶段 | 关键结果 | 对当前决策的作用 |
| --- | --- | --- |
| direct diagnostic PointNormal 5-run | 256K direct B/A 多数不稳定或负向。`abc-fused` 为 `0.96x / 0.88x`，`abcd-fused` 为 `0.92x / 0.60x`。 | 说明 direct helper 单独不足以支持接入。 |
| production-shaped PointNormal 5-run | `abc-fused` 256K 为 `1.11x / 1.09x`，`abcd-fused` 256K 为 `1.13x / 1.09x`。 | 支持继续补 generic representative 和 production-symbol 证据。 |
| generic fused-formula warm-up 5-run | `abcd` 与 `abcd-ilp` 在三类代表点型的 production-shaped full estimate 上整体更稳。 | 支持把 `abcd` 组合推进到 production-loop。 |
| row-source diagnostic to production loop | `row-sources` 诊断里 source-indexed 行稳定正向，后续补 source-indexed production direct、source-indexed bench 和 repeated board summary。 | 支持把 source-indexed 从 diagnostic candidate 升级成 production adopted。 |
| source-indexed implementation-family comparison | repeated diagnostic negative-with-variance | Phase 030 的 source-indexed block-reduction / fused-formula family comparison 有 5-run summary。`block-fused-abcd-ilp` full estimate median `0.90x`、`4/5` 低于 `1.0x`，doctor 为 `3E / 6W / 13S`。 | Phase 031 后降级为 historical diagnostic / harness-risk signal；真实 production direct probe 未复现该负向。 |
| production-symbol 20-run | `PointNormal -> PointNormal` avg B/A median 为 `0.976x`，avg 低于 `1.0x` 为 `12/20`。两个 `PointXYZ` 代表组合平均正向。 | 记录 accepted risk。 |
| production-default repeated summary | 三类代表点型 std/RVV speedup 均正向。 | 支撑当前 production adopted 结论。 |

`AbcdFused` 与 `AbcdFusedIlp` 的当前 asm 统计相同。`AbcdFusedIlp` 的采用理由是源码显式保留独立 seed multiply、点差和累加阶段，便于维护者审查数据依赖。这里声明 code-shape preference，不声明独立机器码收益。

## Traceability Map

| 符号 / 文件 | 层级 | 作用 | 证据角色 |
| --- | --- | --- | --- |
| `loadPointToPlaneLLSWeightedFullReductionVectors` | production RVV helper | 当前默认 production 的 `a/b/c/d` 公式和 lane 数据准备。 | production patch、公式实现和 asm 归属。 |
| `buildPointToPlaneLLSWeightedFullCloudBlockRVV` | production RVV helper | full-cloud block-reduction RVV path。 | 默认 production 正确性、追踪、反汇编归因。 |
| `buildPointToPlaneLLSWeightedSourceIndicesBlockFusedAbcdIlpRVV` | production RVV helper | Phase 031 source-indexed block-fused production probe path。 | source-indexed correctness、probe bench 和 repeated board。 |
| `buildPointToPlaneLLSWeightedSourceIndicesStagedRVV` | production RVV helper | source-indexed staged-gather rollback path。 | prior source-indexed correctness、bench 和 repeated board。 |
| `buildPointToPlaneLLSWeightedFullCloudDefault` | production selector | RVV 可用时用 RVV，否则 std。 | 真实生产路径测试和回退路径测试。 |
| `buildPointToPlaneLLSWeightedSourceIndicesDefault` | production selector | source-indexed RVV 可用时先用 block-fused probe，失败后用 staged-gather，再失败时用 std。 | source-indexed production tests。 |
| `estimateRigidTransformation(cloud_src, cloud_tgt, matrix)` | public entry | full-cloud public overload，会尝试 RVV dispatch。 | production-dispatch bench。 |
| `estimateRigidTransformation(cloud_src, indices_src, cloud_tgt, matrix)` | public entry | source-indexed public overload；当前同样尝试 RVV dispatch。 | source-indexed production bench。 |
| `src/test_teptplw_input_semantics.cpp` | gtest | 数量不匹配、0 权重和负权重 public semantics。 | 输入语义正确性。 |
| `src/test_teptplw_production_direct.cpp` | gtest | production direct、layout gate 和 fallback。 | 生产路径正确性。 |
| `include/impl/teptplw_bench_cases.hpp` | bench registry | case-filter 到 bench case 的映射。 | bench label source of truth。 |
| `include/impl/teptplw_bench_harness.hpp` | bench harness | `run_case`、`run_case_trace`、checksum 输出。 | bench 计时和 checksum。 |
| `script/collect_teptplw_board_rvv_ba.py` | topic-local script | 多轮上板、raw log、checksum 和 summary 生成。 | repeated board evidence。 |
| `script/summarize_teptplw_trace.py` | topic-local script | raw trace log 汇总。 | 追踪摘要。 |
| `script/generate_teptplw_asm_attribution.py` | topic-local script | 生成符号级 asm 归因。 | 路径和指令证据。 |

更完整的函数族说明见 `test-support-code-map.zh.md`。

## EvidenceDecision Audit

支持当前 production adopted 的条件：

- full-cloud public overload 已有真实 direct tests。
- size、weights、VL、layout、byte-offset、`Scalar` 和 small-input fallback 均有测试或源码审查。
- QEMU correctness 已覆盖 47 个 gtest。std 构建为 `45 passed + 1 skipped`，RVV 构建全部通过。
- production-default asm attribution 已记录实际 `boundary`，覆盖默认 RVV path 的归因边界。
- production-dispatch repeated board summary 在三类代表点型、262144 点上正向。
- production-source-indices block-fused probe repeated board summary 在三类代表点型、65536 和 262144 点上正向；对应 Evidence Doctor 为 0 Errors / 9 Warnings / 12 Suggestions，warnings 主要记录 source-indexed-specific asm boundary、262144 长尾，suggestions 包含 binary identity / taskset metadata 未闭合。
- production-default trace 经过 5 runs，每轮 20 次测量和 5 次 warm-up，三类点型 checksum 序列一致。

证据不支持的扩展：

- dual-indices production：当前只有 public semantics、row-source correctness 和 diagnostic bench 入口；缺 production direct/fallback、符号级生产归属和 repeated board。
- correspondences production：当前只有 public semantics、row-source correctness 和 diagnostic bench 入口；还缺 index/weight 展开消融、production direct/fallback、符号级生产归属和 repeated board。
- row-source diagnostic manifest：只保留 pre-production observed speedup 和诊断边界；不能替代 dual-indices / correspondences 的同 family candidate、test、bench、board 和 asm 证据。
- source-indexed `block-fused-abcd-ilp` clean adoption：当前 production probe 为 positive-with-warnings，但缺 asm attribution、binary identity、taskset metadata 和 extended-run，不能写成 clean adopted。
- `Scalar=double` RVV：当前 production helper 只批准 `Scalar=float`。
- 未逐类型上板的 gate-allowed 点型性能。

因此当前 EvidenceDecision 保持为：

```text
bounded-production-candidate/full-cloud-adopted-and-source-indexed-block-fused-probe-f32-aos-valid-index-positive-with-warnings
```

## 遗留风险

- 代表点型只证明三类组合。新增 gate-allowed 点型的性能结论需要对应 board evidence。
- 接入前 `PointNormal -> PointNormal` 的 production-symbol fused-vs-block 20-run 存在 `B/A < 1` 高频风险。当前 production-dispatch 5-run std/RVV summary 均正向，但不证明长期无波动。
- dual-indices / correspondences 的负向归因尚未消融，不能写成单一主因。
- source-indexed 当前路径先走 block-fused probe，再保留 staged-gather / compressed-tail rollback。Phase 031 production direct 与 Phase 030 diagnostic 分叉，说明仍缺 raw per-run trace、环境字段、binary hash、source-indexed-specific asm 和同边界消融，不能写成细粒度根因或 clean adopted。
- empty input、invalid indices 和 invalid correspondences 没有 public semantics 测试。后续应先审计 upstream API 语义。
