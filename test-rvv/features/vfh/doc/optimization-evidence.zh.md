# VFH 优化证据索引

## 当前采用路线

| candidate family | 状态 | production / test 位置 | 关键证据 | 边界 |
| --- | --- | --- | --- | --- |
| `vfh-production-rvv-bin-index-precompute` | adopted production behavior（已采用生产行为） | `features/include/pcl/features/impl/vfh.hpp` 的 `computeVFHSignatureRVV()`、`accumulateVFHSPFHRVV()`、`accumulateVFHViewpointRVV()` | Phase 060 post-review `production_vfh_compute_default` mean `1.63906x`，checksum 一致，Doctor `0E/0W/11S`。 | 默认 VFH public path；不覆盖 `normalize_bins=false`、`size_component`、`normalize_distances`、非 dense / subset indices、CVFH / OUR-CVFH。 |

## 已尝试并被替代路线

| candidate family | 状态 | 结果 | 被替代原因 |
| --- | --- | --- | --- |
| `vfh-centroid-spfh-rvv` | positive diagnostic | Phase 020 candidate mean `2.628x`。 | 只覆盖 test-only pair math，后续 viewpoint / normal centroid 更完整。 |
| `vfh-viewpoint-histogram-rvv` | positive diagnostic | Phase 020 combined mean `2.884x`。 | Phase 030 normal centroid reduction 进一步降低 candidate 耗时。 |
| `vfh-centroid-normal-reduction-rvv` | positive diagnostic | Phase 030 candidate mean `3.018x`。 | 作为 production probe 来源，但不是 production direct 证据。 |
| `vfh-production-probe-phase040` | positive production-public / superseded | Phase 040 production mean `1.36195x`。 | whole-cloud staging 有额外 O(N) 内存流量。 |
| `vfh-production-chunk-local-staging` | positive production-public / superseded | Phase 050 production mean `1.44115x`。 | Phase 060 RVV bin index precompute 显著更快。 |

## 暂缓或不建议继续路线

| 路线 | 状态 | 原因 | 恢复条件 |
| --- | --- | --- | --- |
| histogram scatter RVV | rejected/not_now | 45/128-bin histogram increment 有同 bin 冲突和浮点累加顺序风险；当前 Phase 060 post-review 已有 `1.63906x` production speedup。 | profile 或消融证明 scatter 是主成本，并接受私有 bin / 分块合并的数值审计。 |
| shared `computePairFeatures` production change | rejected/not_now | 共享 helper 会影响 PFH / FPFH / PFHRGB / VFH 全家族，且无独立批量入口。 | 多个 caller-shaped topic 都证明同一 batch helper 可维护后另开共享 helper topic。 |
| point type / parameter expansion | deferred / separate follow-up | 当前 adopted 结论是有界生产行为，不能外推到未测试点型、`normalize_bins=false`、`size_component`、`normalize_distances` 或 CVFH / OUR-CVFH。 | 每个扩展维度新建 phase，补 correctness、fallback、asm、board 和 Evidence Doctor。 |

## 证据路径

| 证据 | 路径 | 作用 |
| --- | --- | --- |
| Phase 060 result | `doc/phases/060-rvv-bin-index-precompute/result.zh.md` | 当前采用路线的阶段结果和 doc-suite parity 审计。 |
| Optimization matrix | `doc/phases/optimization-matrix.zh.md` | 跨阶段 candidate / decision 状态。 |
| Production board output | `log/board/repeated-production-phase060-postreview` | 当前生产性能数据来源。 |
| Production topic doc | `doc-rvv/features/vfh-RVV.zh.md` | 长期维护者读取的 adopted production behavior。 |
