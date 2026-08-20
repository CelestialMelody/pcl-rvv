# bilateral_upsampling 测试、bench 与证据

本文承担 `benchmark_and_evidence` role：解释 bench 输出、case label、计时边界、QEMU / board 证据角色、manifest / Evidence Doctor 和提交边界。correctness 细节见 `doc/correctness-tests.zh.md`，候选族取舍见 `doc/optimization-evidence.zh.md`。

## 入口分类

| target | 主测试类型 | 主要日志 / summary | 证据边界 |
| --- | --- | --- | --- |
| `run_test_compare` | correctness（正确性）与边界测试 | `log/qemu/run_test_std.log`、`log/qemu/run_test_rvv.log` | QEMU 只证明功能、fallback 和日志形状，不证明性能。 |
| `dump_bench_rvv` | asm attribution（反汇编归因）输入 | `build/asm/riscv/bench_bilateral_upsampling_rvv.asm` | 只能说明 RVV 指令存在；还需要人工归属到 candidate helper。 |
| `board_smoke` | board performance（板卡性能）与板卡 correctness | `log/board/run_test.log`、`log/board/analyze_bench_compare.log` | 性能结论只来自板卡；单次 smoke 之后需按 phase plan 判断是否复跑。 |

## CLI 与输出格式

`src/bench_bilateral_upsampling.cpp` 的 CLI 是 `bench_bilateral_upsampling [iterations] [warmup]`。当前 phase 072 manifest 记录 `iterations=5`、`warmup_iterations=2`、`run_count=1`。bench binary 固定按源码中的 case 顺序运行，没有 case-filter CLI；因此 case 隔离依赖输出 label、phase 文档和 Evidence Doctor manifest，而不是命令行过滤。

每个 case 输出 average timing（平均耗时）和 checksum。checksum 用于同一 build family 内的输出形状和结果漂移检查；性能结论只使用板卡 Std/RVV 同 label 比较，speedup 为 `Std avg / RVV avg`。

## bench case

| case | 输入 | 计时边界 | 能证明 | 不能证明 |
| --- | --- | --- | --- | --- |
| `bilateral upsampling table window 80x60 w3 dense` | 无 NaN 的合成 RGBD grid | 表已预计算；包含窗口扫描、查表、累加和输出反投影 | 小图 dense 窗口候选收益 | 真实 production 点型布局 |
| `bilateral upsampling table window 120x90 w4 holes` | 带规律 NaN holes 的合成 RGBD grid | 同上，包含 finite skip | NaN fallback 与中等窗口 | 真实 sensor 分布 |
| `bilateral upsampling table window 180x120 w5 dense` | 更大 dense grid | 同上 | 大窗口趋势 | 完整工具 `tools/bilateral_upsampling.cpp` 调用链 |
| `bilateral upsampling direct-depth 80x60 w3 dense` | 无 NaN 的合成 RGBD grid | 标量查表 weight staging + RVV strided depth load + reduction + unprojection | direct-depth 小图趋势 | strict production A/B |
| `bilateral upsampling direct-depth 120x90 w4 holes` | 带规律 NaN holes 的合成 RGBD grid | 同上，含 NaN mask merge | direct-depth NaN fallback 与中等窗口趋势 | 真实 `PointXYZRGB/RGBA` layout |
| `bilateral upsampling direct-depth 180x120 w5 dense` | 更大 dense grid | 同上 | direct-depth 大窗口趋势 | public entry dispatch 成本 |
| `bilateral upsampling production public PointXYZRGB 80x60 w3 dense` | `PointXYZRGB` 生产公开入口 | 包含 public overload、对象状态、fallback、已有 stdout 和 unprojection | production public 小图边界 | RVV 族采纳 |
| `bilateral upsampling production public PointXYZRGB 120x90 w4 holes` | `PointXYZRGB` 生产公开入口 | 同上 | production public 中图边界 | RVV 族采纳 |
| `bilateral upsampling production public PointXYZRGBA 180x120 w5 dense` | `PointXYZRGBA` 生产公开入口 | 同上 | production public 大图边界 | RVV 族采纳 |
| `bilateral upsampling production public PointXYZRGB to PointXYZRGBA 120x90 w4 holes` | `PointXYZRGB -> PointXYZRGBA` 生产公开入口 | 同上 | cross RGB/RGBA holes public 边界 | 其它点型、alpha 写回语义 |
| `bilateral upsampling production public PointXYZRGBA to PointXYZRGB 120x90 w4 dense` | `PointXYZRGBA -> PointXYZRGB` 生产公开入口 | 同上 | cross RGB/RGBA dense public 边界 | 其它点型 |
| `bilateral upsampling production steady public PointXYZRGB 80x60 w3 dense` | `PointXYZRGB` steady-state public entry（稳态公开入口） | 对象构造和 setters 在 timed window 外，`process` 内 stdout 临时静默 | public shell overhead ablation | production adoption |
| `bilateral upsampling production steady public PointXYZRGB 120x90 w4 holes` | `PointXYZRGB` steady-state public entry | 同上 | public shell overhead ablation | production adoption |
| `bilateral upsampling production steady public PointXYZRGBA 180x120 w5 dense` | `PointXYZRGBA` steady-state public entry | 同上 | public shell overhead ablation | production adoption |
| `bilateral upsampling production steady public PointXYZRGB to PointXYZRGBA 120x90 w4 holes` | `PointXYZRGB -> PointXYZRGBA` steady-state public entry | 同上 | cross RGB/RGBA steady public 边界 | 其它点型、alpha 写回语义 |
| `bilateral upsampling production steady public PointXYZRGBA to PointXYZRGB 120x90 w4 dense` | `PointXYZRGBA -> PointXYZRGB` steady-state public entry | 同上 | cross RGB/RGBA steady public 边界 | 其它点型 |
| `bilateral upsampling production detail helper PointXYZRGB 80x60 w3 dense` | `PointXYZRGB` production detail helper | legacy compatibility label，当前实际走 color-gather helper | production detail helper-only ablation | production adoption |
| `bilateral upsampling production detail helper PointXYZRGB 120x90 w4 holes` | `PointXYZRGB` production detail helper | legacy compatibility label，当前实际走 color-gather helper | production detail helper-only ablation | production adoption |
| `bilateral upsampling production detail helper PointXYZRGBA 180x120 w5 dense` | `PointXYZRGBA` production detail helper | legacy compatibility label，当前实际走 color-gather helper | production detail helper-only ablation | production adoption |
| `bilateral upsampling production detail local nan-mask k64 PointXYZRGB 80x60 w3 dense` | `PointXYZRGB` production detail helper | local helper direct call | mask/chunk ablation | production adoption |
| `bilateral upsampling production detail local nan-mask k64 PointXYZRGB 120x90 w4 holes` | `PointXYZRGB` production detail helper | local helper direct call | mask/chunk ablation | production adoption |
| `bilateral upsampling production detail local nan-mask k64 PointXYZRGBA 180x120 w5 dense` | `PointXYZRGBA` production detail helper | local helper direct call | mask/chunk ablation | production adoption |
| `bilateral upsampling production detail color-gather PointXYZRGB 80x60 w3 dense` | `PointXYZRGB` bench-local color-gather helper | helper direct call with vector RGB gather | color-gather precursor | production public adoption；只证明值得进入 phase 070 |
| `bilateral upsampling production detail color-gather PointXYZRGB 120x90 w4 holes` | `PointXYZRGB` bench-local color-gather helper | 同上，含 holes | color-gather precursor with NaN holes | production public adoption；只证明值得进入 phase 070 |
| `bilateral upsampling production detail color-gather PointXYZRGBA 180x120 w5 dense` | `PointXYZRGBA` bench-local color-gather helper | 同上，RGBA 点型 | color-gather precursor large case | production public adoption；只证明值得进入 phase 070 |

## 当前板卡结果

下面的 phase 020/030/040/050 结果现在都应视作旧 family 的历史基线：helper-only 两次 bounded run 方向冲突，local nan-mask k64 只在大 RGBA case 上略过 1.0x，整体不支持旧 helper family。采用依据以 phase 060/070 的 same-type color-gather family 为准。

`make -C test-rvv/surface/bilateral_upsampling board_smoke` 已在 Milkv-Jupiter 上完成。RVV staged-window-reduction 候选三项均退化：

| case | speedup | Evidence Doctor 处理 |
| --- | ---: | --- |
| `80x60 w3 dense` | 0.90x | `ba_degradation_frequency` Error，降级为 no-production。 |
| `120x90 w4 holes` | 0.91x | `ba_degradation_frequency` Error，降级为 no-production。 |
| `180x120 w5 dense` | 0.90x | `ba_degradation_frequency` Error，降级为 no-production。 |

Evidence Doctor（证据体检）输入为 `doc/phases/000-current-state-and-diagnostic-staged-window/evidence_manifest.json`，输出为同目录 `evidence-doctor.md`。Warnings 来自 diagnostic A/B 边界不同，不能外推为 production evidence。

Phase 010 direct-depth 候选同一次板卡 smoke 中表现为正向：

| case | Std avg | RVV avg | speedup | Evidence Doctor 处理 |
| --- | ---: | ---: | ---: | --- |
| `direct-depth 80x60 w3 dense` | 4.8879 ms | 4.8537 ms | 1.01x | `near_threshold_ba` Suggestion，弱正向，PI1 需复核。 |
| `direct-depth 120x90 w4 holes` | 19.0473 ms | 17.0909 ms | 1.11x | positive diagnostic。 |
| `direct-depth 180x120 w5 dense` | 62.0909 ms | 49.2475 ms | 1.26x | positive diagnostic。 |

Phase 010 Evidence Doctor 输入为 `doc/phases/010-column-stride-depth-direct-probe/evidence_manifest.json`，输出为同目录 `evidence-doctor.md`，结果为 `Errors=0`、`Warnings=12`、`Suggestions=1`。Warnings 来自 wrapper、timer boundary、mask 和 reduction 与 baseline 不同；它们不阻塞 diagnostic 结论，但阻止把该结果写成 strict production performance evidence。

Phase 020 production public probe 同一板卡 smoke 中表现为负向：

| case | Std avg | RVV avg | speedup | Evidence Doctor 处理 |
| --- | ---: | ---: | ---: | --- |
| `production public PointXYZRGB 80x60 w3 dense` | 6.8092 ms | 7.0564 ms | 0.96x | `ba_degradation_frequency` Error |
| `production public PointXYZRGB 120x90 w4 holes` | 27.1604 ms | 30.5335 ms | 0.89x | `ba_degradation_frequency` Error |
| `production public PointXYZRGBA 180x120 w5 dense` | 80.2801 ms | 84.7961 ms | 0.95x | `ba_degradation_frequency` Error |

Phase 020 Evidence Doctor 输入为 `doc/phases/020-column-stride-depth-production-probe/evidence_manifest.json`，输出为同目录 `evidence-doctor.md`，结果为 `Errors=3`、`Warnings=9`、`Suggestions=0`。这些 Error 说明旧 exact-gate family 的公开入口每个 case 都低于标量，不能把那条历史 production helper 写成 adopted。

Phase 030 steady-state public shell ablation 也已完成：

| case | Std avg | RVV avg | speedup | Evidence Doctor 处理 |
| --- | ---: | ---: | ---: | --- |
| `steady public PointXYZRGB 80x60 w3 dense` | 7.3080 ms | 7.4799 ms | 0.98x | `ba_degradation_frequency` Error |
| `steady public PointXYZRGB 120x90 w4 holes` | 27.4150 ms | 30.2814 ms | 0.91x | `ba_degradation_frequency` Error |
| `steady public PointXYZRGBA 180x120 w5 dense` | 81.1577 ms | 82.5887 ms | 0.98x | `ba_degradation_frequency` Error |

Phase 030 新增 steady-state public 消融 case 后，`dump_bench_rvv`、`run_test_compare` 和板卡 `board_smoke` 均完成；反汇编仍能归属到生产 helper。`run_bench_rvv BENCH_ARGS='1 0'` 已作为 QEMU bench smoke（QEMU 小型 bench 冒烟，只看日志形状）运行，新增 steady-state case 的 checksum 和误差输出可见，但 QEMU timing 不作为性能结论。phase 030 board 结果为 `0.98x / 0.91x / 0.98x`，Evidence Doctor 为 `Errors=3`、`Warnings=9`、`Suggestions=0`；这说明 public shell overhead ablation 没有把当前生产补丁救成可采纳。当前仍不能用 phase 030 改写 phase 020 的负向 production 结论。

Phase 040 helper-only bounded board run 曾出现 `1.07x / 1.02x / 1.01x`，但同一 phase 的第二次 bounded rerun 翻成 `0.94x / 0.90x / 0.95x`，Evidence Doctor 为 `Errors=3`、`Warnings=13`、`Suggestions=1`。这说明 helper 本体也不稳定，不应再把 helper-only 的早期正向当 current truth。

Phase 050 继续做 mask/chunk 消融，新增 local nan-mask k64 变体。该历史 board 结果为 `0.98x / 0.92x / 1.01x`，Evidence Doctor 为 `Errors=2`、`Warnings=4`、`Suggestions=1`；它只在大 RGBA case 上略过 1.0x，不能把旧 production helper 抬成 adopted，也不能证明 strict finite mask 是唯一主因。

Phase 060/070 切换到 color-gather family 后，same-type board 采用证据已经转正并进入 adopted 语义。060 的 bench-local direct helper 先给出 `1.31x / 1.28x / 1.07x`，随后 070 把同 family 接进 production helper。Phase 071 补 finite mask，phase 072 收窄 same-type gate，phase 073 又扩展 cross RGB/RGBA 后，当前 production public / steady board 结果为：

| case | Std avg | RVV avg | speedup | 备注 |
| --- | ---: | ---: | ---: | --- |
| `production public PointXYZRGB 80x60 w3 dense` | 6.2335 ms | 5.0848 ms | 1.23x | phase 073 current evidence |
| `production public PointXYZRGB 120x90 w4 holes` | 25.5946 ms | 23.4010 ms | 1.09x | phase 073 current evidence |
| `production public PointXYZRGBA 180x120 w5 dense` | 74.7805 ms | 66.1979 ms | 1.13x | phase 073 current evidence |
| `production public PointXYZRGB -> PointXYZRGBA 120x90 w4 holes` | 25.7701 ms | 23.3797 ms | 1.10x | phase 073 current evidence |
| `production public PointXYZRGBA -> PointXYZRGB 120x90 w4 dense` | 25.8729 ms | 23.4927 ms | 1.10x | phase 073 current evidence |
| `production steady public PointXYZRGB 80x60 w3 dense` | 6.6024 ms | 5.4519 ms | 1.21x | phase 073 current evidence |
| `production steady public PointXYZRGB 120x90 w4 holes` | 26.0561 ms | 23.8677 ms | 1.09x | phase 073 current evidence |
| `production steady public PointXYZRGBA 180x120 w5 dense` | 77.4845 ms | 66.8512 ms | 1.16x | phase 073 current evidence |
| `production steady public PointXYZRGB -> PointXYZRGBA 120x90 w4 holes` | 26.0141 ms | 23.8979 ms | 1.09x | phase 073 current evidence |
| `production steady public PointXYZRGBA -> PointXYZRGB 120x90 w4 dense` | 26.1494 ms | 23.7843 ms | 1.10x | phase 073 current evidence |

Phase 073 Evidence Doctor 对当前 manifest 报告了 `Errors=0`、`Warnings=0`、`Suggestions=0`。Phase 070 manifest 是历史采纳证据，曾报告 `Errors=0`、`Warnings=0`、`Suggestions=1`，唯一 suggestion 来自 180x120 RGBA public case 的 `1.02x`；phase 072 刷新后该 case 曾为 `1.21x`，phase 073 当前二进制仍为正向。

Phase 071 之后，当前二进制修复了 finite mask，使 color-gather helper 与标量 `std::isfinite` 一样跳过 NaN 和 infinity。Phase 072 把 production RVV gate 收窄到 same-type RGB/RGBA，phase 073 又用交叉 production direct 证据把 gate 扩展到 RGB/RGBA exact family。本轮已刷新 `run_test_compare`、`dump_bench_rvv`、QEMU bench log-shape smoke 和板卡 `board_smoke`；phase 073 数字是当前二进制的新鲜性能结果。

## ASM 与提交边界

当前 RVV asm 归属以 `build/asm/riscv/bench_bilateral_upsampling_rvv.asm` 为入口。adopted color-gather family 需要能看到 `vlse8.v`、`vzext.vf2`、`vmaxu.vv`、`vminu.vv`、`vluxei16.v` 和 `vfredusum.vs`；phase 071/072 后还应能看到 finite mask 的 `vfabs.v` / `vmflt.vf`。旧 helper family 的 `vlse32/vmfeq/vmerge/vfmul/vfredusum` 只能作为历史对照。

默认可提交或长期引用的是 summary、manifest、doctor、QEMU correctness log 和必要的 asm 摘要。raw board log、本地 build 产物、私有板卡地址和未脱敏路径默认排除。当前没有 `evidence_registry.json`，Handoff 中用 `evidence_registry_status=not_available` 和手工检查路径说明边界。
