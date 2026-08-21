# debayer optimization matrix

| candidate family | entry / layout | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| bilinear-inner-u8-stencil-strided-store | `DeBayer::debayerBilinear` full-size contiguous Bayer 内区，12 次 `vsse8` 写回 | pass: `run_test_compare` | qemu_smoke_only | negative: 0.94x / 0.95x | pass: `dump_bench_rvv` | warning: negative + metadata incomplete | rejected | none inside current family |
| bilinear-inner-u8-stencil-segmented-store | 同边界，2 次 `vsseg6e8` 写回 | pass: `run_test_compare` | qemu_smoke_only | negative: 0.93x | pass: `dump_bench_rvv` | warning: negative + metadata incomplete | rejected | none inside current family |
| bilinear-boundary-lines | 首末两行 / 首末两列标量边界 | not_applicable for phase 000 | not_applicable | not_applicable | not_applicable | not_applicable | rejected for now | 只有出现 positive 内区 family 才进入 production boundary split。 |
| edge-aware-inner | `debayerEdgeAware` 内区 `abs` 分支 | not_run | not_run | not_run | not_run | not_run | deferred with resume condition | 需要 profile 或新候选证明分支成本可摊薄。 |
| weighted-inner | `debayerEdgeAwareWeighted` 内区 `abs` / 除法 | not_run | not_run | not_run | not_run | not_run | deferred with resume condition | 需要先有 edge-aware/profile 正向证据。 |

## Continue / stop decision

当前矩阵没有授权且未阻塞的 high-priority next action（高优先级下一动作）。`debayerBilinear` 两个同边界 candidate 已被板卡 negative 证据拒绝；`edge-aware-inner` 和 `weighted-inner` 仍是带恢复条件的 deferred（暂缓）项，不是可以直接进入实现或 production 的未阻塞项。默认停止条件为 `no-production for bilinear inner family`。
