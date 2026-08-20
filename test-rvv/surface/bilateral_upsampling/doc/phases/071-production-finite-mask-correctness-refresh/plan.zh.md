# Phase 071 plan: production finite-mask correctness refresh

## 阶段意图和边界

本阶段是在 adopted color-gather production helper 上做 correctness refresh（正确性刷新）：closeout 审计发现 color-gather helper 的 finite mask 只用 `z == z` 过滤 NaN，而标量路径使用 `std::isfinite`，需要同时跳过 NaN 和 infinity。

本阶段不改变 adopted family 的核心组织方式，不重开 helper-only / nan-mask 路线；只把 production color-gather helper、同语义 test support helper 和 direct-depth diagnostic helper 的 finite mask 对齐到标量语义。

## 实现动作

1. 在 production color-gather helper 中把 `finite = z == z` 扩展为 `z == z && abs(z) < infinity`。
2. 在 bench-local color-gather helper 中同步同一语义，避免 test support 与 production 分叉。
3. 在 direct-depth diagnostic helper 中同步同一语义，保持 diagnostic correctness 可信。
4. 新增 infinity correctness tests，覆盖 diagnostic direct-depth 和 production public entry。

## 验证计划

| action | 命令 | 证据角色 |
| --- | --- | --- |
| QEMU correctness | `make -C test-rvv/surface/bilateral_upsampling run_test_compare` | Std/RVV correctness，预期从 9/9 扩到 11/11 |
| asm refresh | `make -C test-rvv/surface/bilateral_upsampling dump_bench_rvv` | 确认 color-gather 指令仍存在，并出现 finite mask 的 `vfabs` / `vmflt` |
| board freshness | `make -C test-rvv/surface/bilateral_upsampling board_smoke` | 需要板卡恢复后刷新；若不可达，写 `board_refresh_pending` |

## 停止条件

如果 QEMU correctness 或 asm attribution 失败，必须先修复实现。若只有板卡 SSH 不可达，则保留当前 patch、记录恢复条件，不把 QEMU timing 写成性能结论。
