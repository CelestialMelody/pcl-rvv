# 优化证据索引

| candidate family | 状态 | 代码路径 | test target | bench / board target | asm / doctor | 证据边界 |
| --- | --- | --- | --- | --- | --- | --- |
| `public-inline-filter-rvv` | adopted | `recognition/include/pcl/recognition/hv/occlusion_reasoning.h` | `run_test_compare`、`ProductionInlineFilterHitsRvvPathAndKeepsExpectedPoints`、`ProductionInlineGetOccludedCloudHitsRvvPathAndKeepsExpectedPoints` | `inline_board_repeated` / `record_inline_evidence_state_repeated` | `check_inline_filter_rvv_asm` / `inline_evidence_doctor_repeated` | production-public；板卡 `1.250x`，`5/5` 稳定正向 |
| `production-projection-filter-rvv` | adopted | `recognition/include/pcl/recognition/impl/hv/occlusion_reasoning.hpp` | `run_test_compare`、`ProductionZBufferingFilterHitsRvvPathAndKeepsExpectedIndices` | `board_repeated` / `record_evidence_state_repeated` | `check_occlusion_filter_rvv_asm` / `evidence_doctor_repeated` | production-public；板卡 `1.270x`，`5/5` 稳定正向 |
| `compute-depth-map-semantics-fix` | adopted | 同上 | `ProductionDepthMapRectangularResolutionRegression` | 已并入同一 board direct 证据链 | 同上 | correctness support；修复矩形 depth map 索引和初始化 |
| `smooth-window-min-rvv` | deferred | 生产源码未改 | not_yet_covered | not_yet_covered | not_yet_covered | 平滑窗口是邻域 min，需独立 caller 证据 |

说明：当前 production path 已经包含 `vcompress` staging + scalar tail，不再把
`projection-filter-vcompress` 当成独立 family。若要再扩展，应另起新的证据边界，而不是在
当前 adopted path 上继续堆包装。
