# Phase 050: production detail mask/chunk ablation

## 阶段意图和边界

Phase 040 把计时边界切到 production detail helper（生产内部 helper）后，首次 board run 为
`1.07x / 1.02x / 1.01x`，第二次 bounded rerun（有界复跑）翻成 `0.94x / 0.90x / 0.95x`。
Evidence Doctor（证据体检）输出 `Errors=3`、`Warnings=13`、`Suggestions=1`，说明当前 helper-only
证据不稳定且最新 run 负向，不能支撑采纳当前生产补丁。

本阶段继续做 bench-only（只改性能测试资产）的 production detail component ablation（生产内部组件消融）：
新增一个测试专用 local RVV helper，保持真实 `PointXYZRGB` / `PointXYZRGBA` 点型、预计算 tables /
unprojection 和 direct helper 计时边界，但把当前生产 helper 中的 strict finite mask（严格有限值 mask，
`z == z && abs(z) < inf`）简化为 NaN mask（只过滤 NaN），并把临时 `weights` chunk 上限从 256
收窄到 64。目标是判断当前生产 helper 的负向是否主要来自 strict finite mask / 大 chunk 形态。

validated_scope：organized RGBD grid、`PointXYZRGB` / `PointXYZRGBA` exact gate、`float` xyz/depth、
window_size=3/4/5、当前合成输入只含 finite 和 NaN，不含 infinity。

unvalidated_scope：infinity 输入语义、公开入口 adoption（采纳）、其它点类型、真实 sensor 分布、
最终是否修改 production 源码。

不可触碰路径：本阶段不修改 `surface/include/pcl/surface/impl/bilateral_upsampling.hpp`；只修改
topic-local bench / phase 文档 / manifest。

## 当前状态清单

| area | 当前事实 | 路径 |
| --- | --- | --- |
| current production helper | strict finite mask + `kMaxChunkLanes=256` + `vlse32` + `vfredusum` | `surface/include/pcl/surface/impl/bilateral_upsampling.hpp` |
| phase 040 helper-only | 两次 board 值为 `1.07/1.02/1.01` 与 `0.94/0.90/0.95`，Evidence Doctor `Errors=3` | `doc/phases/040-production-detail-helper-only-ablation/evidence-doctor.md` |
| phase 010 direct-depth diagnostic | test-local `RgbPoint` 上持续正向，最新 single run 为 `1.11x / 1.20x / 1.34x` | `log/board/analyze_bench_compare.log` |

## 假设与候选

| candidate family | 假设 | 预期信号 | 风险 |
| --- | --- | --- | --- |
| production detail nan-mask-k64 | 如果 strict finite mask 和 256-lane stack frame 是主要成本，NaN-only + k64 变体应明显好于 current production helper | 三个 helper-only case 从负向恢复到稳定正向，至少大中 case > 1.05x | 只覆盖 NaN，不覆盖 infinity；即使正向也只能进入下一 phase 的 strict finite 语义恢复设计 |
| current production helper negative | 如果 local nan-mask-k64 仍负向，说明主要成本不在 strict mask/chunk，而在生产点型布局、weight staging 或 vector reduction | 三项仍 `<= 1.0x` 或不稳定 | 需要停止当前实现族，等待用户确认回滚或另寻完全不同候选 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| production detail nan-mask-k64 | organized RGBD grid | `PointXYZRGB` / `PointXYZRGBA`, production AoS layout | test-local direct helper, precomputed tables/unprojection | existing production public correctness + bench checksum/error | new `production detail local nan-mask k64` cases | pending | local helper should contain `vlse32/vmfeq/vmerge/vfmul/vfredusum` | pending | planned | add bench cases, run QEMU smoke, asm, board, doctor |

## 实现和测试动作

| action | 产物 / 命令 | 完成判据 |
| --- | --- | --- |
| add local ablation helper | `src/bench_bilateral_upsampling.cpp` | 新增测试专用 helper 和三条 `production detail local nan-mask k64` case。 |
| QEMU correctness / smoke | `make -C test-rvv/surface/bilateral_upsampling run_test_compare`; `make -C ... run_bench_rvv BENCH_ARGS='1 0'` | correctness 仍 9/9；QEMU bench 只检查新增 case 输出形状，不采信 timing。 |
| asm refresh | `make -C test-rvv/surface/bilateral_upsampling dump_bench_rvv` | 反汇编能看到目标 RVV 指令，归属到 bench/local helper 或内联片段。 |
| board refresh | `make -C test-rvv/surface/bilateral_upsampling board_smoke` | 得到新增 local nan-mask-k64 三项板卡结果。 |
| doctor refresh | 生成 phase 050 manifest 并运行 Evidence Doctor | 回填 Errors / Warnings / Suggestions 和是否值得进入 strict finite 语义恢复 candidate。 |

## 诊断到生产错配审计

| question | answer |
| --- | --- |
| evidence role | production-detail-ablation（生产内部 helper 消融） |
| A/B boundary | test-local production detail helper；预计算 tables/unprojection，不含 public `process()` shell |
| 当前决策问题 | implementation-shape：strict finite mask / chunk 形态是否是当前 helper 负向主因 |
| diagnostic 是否可外推到 production | 不能直接外推到 adoption；它只能决定是否值得设计新的 strict-finite production candidate。 |
| comparison-boundary / baseline mismatch 风险 | 有；NaN-only mask 有意不等价于 production `std::isfinite`。 |
| weak / negative / neutral / unstable 时是否允许 bounded probe | 允许一次 board smoke；若仍负向或不稳定，不再扩大当前实现族。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 是。即使本阶段正向，也必须先恢复 infinity 语义并重新跑 production public PI2-PI5。 |

## 板卡复跑预算和决策桶

本阶段预算为一次 `board_smoke`。若新增三项明显正向且与 phase 040 最新负向分离，再创建下一 phase
恢复 strict finite 语义；若新增三项仍负向 / 中性 / 不稳定，当前实现族不再继续自动探索。

决策桶：`positive >= 1.10x`，`weak-positive 1.03x..1.10x`，`neutral 0.97x..1.03x`，`negative < 0.97x`。

## 继续 / 停止条件

默认推进到 bench、QEMU、asm、board 和 Evidence Doctor。只有新增 local helper 编译失败且同轮无法修复、
board 不可达、Evidence Doctor Error 暴露证据合同无法修复，或结果表明当前实现族已经没有未阻塞收益路线时，
才停止在用户判断点。

## 文档更新清单

回填 `result.zh.md`、phase README、optimization matrix、optimization roadmap、evaluation、
benchmark-and-evidence、current handoff 和 surface queue。phase 040 的不稳定结果也必须同步成 current truth。
