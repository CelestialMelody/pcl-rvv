# color_gradient_modality 阶段索引

| phase | status | 默认恢复动作 | 说明 |
| --- | --- | --- | --- |
| `000-current-state-and-cgm-scaffold` | completed | 默认继续 `010-dominant-filter-rvv` | Sobel+quantize production-shaped diagnostic 已有 QEMU correctness、asm、5-run board repeated 和 Evidence Doctor；不接 production。 |
| `010-dominant-filter-rvv` | completed | 默认继续 `020-full-chain-production-shaped-diagnostic` | 3x3 dominant filter production-shaped diagnostic 已有 QEMU correctness、asm、5-run board repeated 和 Evidence Doctor；不接 production。 |
| `020-full-chain-production-shaped-diagnostic` | completed | 默认继续 `030-production-integration-plan` | Sobel+quantize+filter 串接 helper repeated board 为 positive，当前结论为 `partial-production-candidate`。 |
| `030-production-integration-plan` | completed/adopted | 默认继续 `040-rgb-stencil-rvv-ablation` | 已完成 production patch、production direct correctness、asm、board repeated 和 Evidence Doctor；用户确认后已采纳并创建正式 `doc-rvv`。 |
| `040-rgb-stencil-rvv-ablation` | completed | 默认继续 `050-rgb-stencil-production-integration` | RGB Sobel stencil RVV diagnostic positive，板卡 repeated median `5.040x` / `4.680x`，支持进入 production integration。 |
| `050-rgb-stencil-production-integration` | completed/adopted | 结束；若未来重开应先做 `extractFeatures()` profile / ablation | RGB Sobel stencil 已接入真实 `processInputData()`，production direct median `1.620x` / `1.590x`，checksum 一致。 |

当前默认恢复入口是 Phase 050 closeout：production 行为已经采纳，正式长期主题文档
`doc-rvv/recognition/color_gradient_modality-RVV.zh.md` 使用 Phase 050 接入后的板卡数据。
若后续继续优化，应先证明 `extractFeatures()` 在完整模板生成中仍是热点，再创建
`060-feature-extraction-profile-ablation`。
