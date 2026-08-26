# VFH Optimization Roadmap

## 当前边界

当前 topic 目标是 `features/include/pcl/features/impl/vfh.hpp`。当前已经采用 Phase 060 的
production RVV（生产 RVV）实现：默认 `VFHEstimation::computeFeature()` 公开入口在满足 dense full-cloud
sequential indices（全点云顺序索引）、float AoS（结构数组）布局、`PointOutT=VFHSignature308`、
`normalize_bins=true`、`normalize_distances=false`、`size_component=false`、未使用给定 centroid / normal 时命中
`pcl::detail::computeVFHSignatureRVV()`。

Phase 060 覆盖 `PointNormal -> VFHSignature308` 测试实例和 traits / layout gate 可证明的 xyz/normal
单 `float` AoS 输入。当前文档不把它外推为所有 VFH 模板实例：非 dense 输入、CVFH / OUR-CVFH callers、
泛型点类型扩展、`Scalar=double`、自定义 VFH 参数、`normalize_bins=false`、`size_component`、`normalize_distances` 和子集 indices
仍保持标量 fallback（回退路径），需要另建 phase 独立批准。

Phase 060 的 post-review production-public board evidence（公开生产入口板卡证据）为 mean speedup `1.63906x`，
5-run checksum 全一致，Evidence Doctor（证据体检）为 `0E/0W/11S`。当前采用的优化覆盖 xyz centroid、
normal centroid、centroid-to-point pair math、viewpoint normal-dot preparation（视点法线点积分箱准备）
和 RVV bin index precompute（RVV 分箱索引预计算）；histogram scatter（直方图离散累加）仍按标量顺序执行。

## 候选搜索空间

| candidate family | idea source | applies to | expected benefit | risk / unknown | required evidence | status | next phase |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `vfh-scalar-reference-scaffold` | 当前源码 shape scan | public compute 与 test-only same-chain reference | 建立后续候选 correctness gate | reference 漏掉 size component、viewpoint bin 或 normalize_distances 会污染证据 | public compute 对拍、size component 对拍 | validated baseline | `000-current-state-and-vfh-scaffold` |
| `vfh-centroid-spfh-rvv` | 筛选清单 + PFH direct-AoS 经验 | `computePointSPFHSignature` centroid-to-point pair math | 已证明 VFH O(N) pair math 可在 test helper 边界内加速 | staging 成本、histogram scatter、public single-descriptor 稀释仍不外推 | candidate correctness、asm、board repeated、Doctor | validated positive diagnostic | `020-viewpoint-histogram-rvv-diagnostic` 已复测 |
| `vfh-viewpoint-histogram-rvv` | `computeFeature()` viewpoint component loop | 128-bin viewpoint histogram normal-dot preparation | 已证明 combined candidate 比 centroid-only 继续降低 RVV 耗时 | histogram scatter 仍标量；production public 仍可能被其它固定成本稀释 | component correctness、bench、board repeated、Doctor | validated positive diagnostic | 已纳入 Phase 030 family |
| `vfh-centroid-normal-reduction-rvv` | `computeFeature()` centroid / normal centroid loop | normal centroid reduction；xyz centroid 已由 common RVV 覆盖 | 已证明 reduction-combined candidate 比 Phase 020 继续降低 RVV 耗时 | 非 dense normals、给定 centroid / normal 和 production dispatch 不外推 | reduction same-chain tests、asm、board repeated、Doctor | validated positive diagnostic | `010-production-integration-plan` 候选范围更新 |
| `vfh-production-probe` | Phase 030 positive diagnostic | bounded production helper under `__RVV10__` | 已证明 production direct 穿透 public boundary，Phase 040 mean `1.36195x` | whole-cloud staging 成本仍可优化 | PI1 plan、production direct tests、asm、board repeated、Doctor | attempted / superseded | Phase 040 已完成 |
| `vfh-production-chunk-local-staging` | Phase 040 result | O(VLmax) chunk-local staging | 已证明减少 O(N) staging 成本，mean speedup `1.44115x` | 每点标量 bin 计算仍在热点 loop | correctness、asm、board repeated、Doctor | attempted / superseded | Phase 050 已完成 |
| `vfh-production-rvv-bin-index-precompute` | Phase 050 result | RVV 预计算 f1/f2/f3/viewpoint bin index，标量顺序 histogram increment | 已证明 mean speedup `1.63906x`，当前采用 | 不覆盖 histogram scatter 并行化、额外参数和点型扩展 | correctness、asm、board repeated、Doctor、doc-rvv closeout | adopted | Phase 060 已完成 |

## 阶段反思新增路线

| phase | new idea | why now | evidence needed | priority |
| --- | --- | --- | --- | --- |
| 000 | phase_000_closed | Phase 000 已完成 correctness、asm、board 和 Doctor。 | result 已回填，candidate 正向。 | high |
| 010 | production_patch_paused | PI1 计划已冻结，但生产源码修改需要用户确认。 | 用户确认后进入 PI2。 | high |
| 020 | viewpoint_route_positive | combined SPFH + viewpoint candidate 比 centroid-only 更快，Doctor 无 Error / Warning。 | 更新 PI1 候选范围；继续 topic-local centroid / normal reduction 诊断。 | high |
| 030 | normal_centroid_route_positive | reduction-combined candidate 比 Phase 020 更快，Doctor 无 Error / Warning。 | 用户确认后把 Phase 030 family 作为 PI2 首选；否则停在授权边界。 | high |
| 040 | production_public_positive | 真实公开入口接入后 mean `1.36195x`，证明 VFH 默认路径有生产收益。 | 继续消除 whole-cloud staging 成本。 | high |
| 050 | chunk_local_staging_positive | chunk-local staging 把 production mean speedup 提到 `1.44115x`。 | 尝试 RVV bin index precompute，保留标量 histogram increment 顺序。 | high |
| 060 | bin_index_precompute_adopted | RVV 分箱索引预计算的 post-review production mean speedup 为 `1.63906x`。 | 当前同边界停止；扩大点型、参数或 histogram scatter 需另起证据闭环。 | closeout |

## 暂缓 / 拒绝路线

| candidate family | reason | resume condition |
| --- | --- | --- |
| `shared-pfh-helper-production-change` | `features/src/pfh.cpp::computePairFeatures` 无本地 batch API，直接修改共享 helper 会影响 PFH/FPFH/PFHRGB/VFH 全家族。 | 只有多个 caller-shaped topic 都证明同一 helper 形态可维护后，再另开共享 helper 主题。 |
| `vfh-histogram-scatter-rvv` | 45/128-bin histogram scatter 有同 bin 冲突和浮点累加顺序风险；Phase 060 post-review 已把算术、规约和分箱闭合到 `1.63906x`，继续 scatter 并行化会显著增加语义风险。 | 只有 profile 或专项消融证明 histogram scatter 是主成本，才另开私有 bin / 分块合并 diagnostic。 |
| `point-type-expansion` | 当前 production gate 只证明 xyz/normal 单 `float` AoS 布局；泛型点类型、`Scalar=double`、`normalize_bins=false`、子集 indices 和 CVFH / OUR-CVFH 不可外推。 | 另开 phase，按点类型 / 参数 / indices 逐项补 correctness、fallback、asm、board 和 Evidence Doctor。 |

## 默认恢复队列

| order | phase | scope | status | resume condition |
| --- | --- | --- | --- | --- |
| 1 | `060-rvv-bin-index-precompute` closeout | 当前默认 VFH production boundary | `adopted / no_unblocked_same_boundary_action` | review / commit 准备；不再继续同边界微优化。 |
| 2 | `point-type-or-parameter-expansion` | 泛型点类型、`size_component`、`normalize_distances`、非 dense / subset indices、CVFH / OUR-CVFH | `deferred / separate follow-up` | 用户选择继续扩大 VFH 覆盖范围时，为每个扩展边界新建 phase。 |
| 3 | `histogram-scatter-ablation` | 私有 bin、分块合并或其它 scatter 方案 | `rejected/not_now` | profile 证明 scatter 是主成本，且用户接受新增数值顺序审计时再恢复。 |
