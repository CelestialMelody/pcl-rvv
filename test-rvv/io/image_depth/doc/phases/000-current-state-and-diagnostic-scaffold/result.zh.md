# Phase 000 结果：当前状态与诊断脚手架

## 执行范围

本阶段按计划只建立 `io/src/image_depth.cpp` 的 production-shaped diagnostic（生产形态诊断）证据链，没有修改 production（生产源码）。实际覆盖范围为：

| 范围 | 状态 | 说明 |
| --- | --- | --- |
| `fillDepthImage` contiguous `xStep == 1` | done | RVV candidate 使用连续 `vle16`、invalid mask（无效像素掩码）和 `vfmul` 写 `float` 米值。 |
| `fillDepthImage` padded output | done | RVV candidate 保持 `line_step` 行尾填充不写，padding 区域由 correctness test（正确性测试）保护。 |
| `fillDisparityImage` contiguous `xStep == 1` | done | RVV candidate 使用连续加载、invalid lane 写 0、有效 lane 执行 `constant / pixel`。 |
| `xStep > 1` downsample | partial | 本阶段只证明 fallback（回退路径）仍走标量参考链路；没有接入 stride-load（跨步加载）RVV。 |
| production direct | not_applicable | 本阶段未改 `io/src/image_depth.cpp`，没有真实公开入口分流证据。 |

## 计划动作回填

| 计划动作 | 状态 | 命令 / 证据路径 | 结论 |
| --- | --- | --- | --- |
| RED 测试 | done | 首次 `make run_test_rvv` 因缺少 `image_depth.h` 失败 | TDD（测试驱动开发）红灯有效，失败指向候选 API 缺失。 |
| 实现候选 helper | done | `include/image_depth.h` | RVV build 仅在 `src_width == width && src_height == height` 时走 contiguous helper，其它入口 fallback 到 scalar。 |
| QEMU correctness | done | `make run_test_rvv`、`make run_test_compare` | RVV 和 Std build 均通过 3 个 gtest；QEMU 只作为正确性和路径证据。 |
| QEMU bench smoke | done | `make run_bench_rvv BENCH_ARGS="--case-filter all --iterations 2 --warmup-iterations 1"` | RVV bench 可运行，日志形状可解析；不作为性能结论。 |
| 反汇编归属 | done | `make dump_bench_rvv`，`build/asm/riscv/bench_image_depth_rvv.asm` | asm 中存在 `vle16`、`vfcvt`、`vfmul`、`vfrdiv`、`vmseq`、`vmor`、`vmnot`、masked `vse32`，归属到 bench 内联 candidate helper。 |
| 板卡 smoke | done | `make check_board_ssh`、`make board_smoke BENCH_ARGS="--case-filter all --iterations 10 --warmup-iterations 2"` | Milkv-Jupiter 可用；全 case 单次 smoke checksum 一致。downsample 结果只作历史诊断，不参与 contiguous 生产候选决策。 |
| repeated board summary | done | `make generate_board_repeated_summary`，`log/board/repeated_contiguous/summary.md` | contiguous 三个主 case 均为 positive（正向）：depth median 1.38x，padded depth median 1.21x，disparity median 1.93x。 |
| Evidence Doctor | done | `log/board/repeated_contiguous/evidence_manifest.json`、`log/board/repeated_contiguous/evidence_doctor.md` | Errors=0，Warnings=1，Suggestions=0；warning 是 disparity 长尾，已保留 min/median/max。 |
| evidence registry | done | `make record_board_repeated_evidence_state`、`make evidence_status`，`log/evidence_registry.json` | 当前 repeated summary、manifest 和 doctor 已登记，registry check 为 fresh。 |

## 诊断证据链

| 证据层 | 当前证据 | 证明什么 | 不能证明什么 |
| --- | --- | --- | --- |
| correctness | `run_test_rvv`、`run_test_compare` 通过 3 个测试 | helper 级 scalar-vs-candidate 对拍覆盖 invalid pixel、NaN/0 输出、padding 和 downsample fallback。 | 不能证明真实 `DepthImage` 公开入口已经分流到 RVV。 |
| QEMU path | `run_bench_rvv` smoke 可运行 | RVV build 的 bench 日志和 wrapper 能运行。 | QEMU timing 不能作为性能结论。 |
| asm attribution | `build/asm/riscv/bench_image_depth_rvv.asm` | candidate helper 的 RVV 指令存在，覆盖 load、mask、convert、mul/div 和 masked store。 | 反汇编不证明 production 符号命中，因为本阶段未接 production。 |
| board performance | `log/board/repeated_contiguous/summary.md` | Milkv-Jupiter 上 contiguous production-shaped helper 相对 scalar reference 明显正向。 | 不能外推到 downsample stride-load、OpenNI legacy 文件或 production direct dispatch。 |
| Evidence Doctor | `log/board/repeated_contiguous/evidence_doctor.md` | 当前 repeated summary 无 Error；disparity 长尾是已解释 warning。 | metadata 仍缺 taskset/governor/freq/temperature/binary hash，不能写成 clean production evidence。 |

## repeated board 结果

| case | runs | median | min | max | decision bucket | 说明 |
| --- | ---: | ---: | ---: | ---: | --- | --- |
| `depth_full_640x480` | 5 | 1.38x | 1.26x | 1.40x | positive | 连续 depth 米值转换收益稳定，高于 1.10x 门槛。 |
| `depth_full_padded_640x480` | 5 | 1.21x | 1.17x | 1.24x | positive | padding 输出仍正向，说明行尾跳过没有抹掉收益。 |
| `disparity_full_640x480` | 5 | 1.93x | 1.64x | 2.04x | positive with warning | 收益明显，但 max/min=1.24，后续 production direct 需要保留长尾解释或扩大 runs。 |

## diagnostic-to-production mismatch audit 回填

| question | result |
| --- | --- |
| evidence role | `production_shaped_diagnostic`，不是 production-public（真实公开入口）证据。 |
| A/B boundary | baseline 是测试专用 scalar reference，candidate 是测试专用 RVV helper；两侧 checksum policy 和 timer boundary 一致。 |
| 当前决策问题 | 当前只回答 contiguous helper 的 RVV-vs-scalar 是否值得进入 bounded production probe（有界生产探针）。 |
| diagnostic 是否可外推到 production | 可以支撑“值得写 PI1 生产接入计划”，不能直接支撑 adoption（采纳）。production 仍需真实入口、fallback、asm 和板卡重跑。 |
| comparison-boundary / baseline mismatch 风险 | helper 不含 `DepthImage` wrapper 虚调用、尺寸异常路径和生产源码分流；PI1 必须冻结这些边界。 |
| 弱 / 负 / 中性 / 不稳定时 bounded production probe | 当前主 case 是 positive，因此允许进入窄范围 PI1；若后续 production direct 降为 weak/negative/unstable，必须暂停在 PI5 用户检查点。 |
| clean adoption 是否需要 production boundary 证据 | 需要。首次候选无已有 adopted RVV family，但 clean adoption 仍必须等 production direct correctness、fallback、asm、repeated board 和用户确认。 |

## Optimization matrix 更新

- `contiguous depth meters RVV`：`attempted -> partial-production-candidate`。正确性、QEMU smoke、asm、board repeated 和 Evidence Doctor 已闭合；production direct 缺口仍未闭合。
- `contiguous disparity RVV`：`attempted -> partial-production-candidate`。收益更高但有长尾 warning；PI4 需要保留 min/median/max 并考虑扩大 runs。
- `downsample stride-load RVV`：保持 `phase_deferred + unblocked`。本阶段只验证 scalar fallback 正确；下一优化 phase 可单独尝试 `vlse16` 或分块 gather，但它不阻塞 contiguous PI1。
- `OpenNI legacy parity`：保持 `deferred`。当前证据不覆盖 `openni_camera/openni_depth_image.cpp`。

## Evidence freshness 与 registry

当前 topic 已接入 `log/evidence_registry.json`。`make record_board_repeated_evidence_state` 会从 `log/board/repeated_contiguous/analyze_bench_compare_*.log` 重新生成 `summary.md`，再生成 `evidence_manifest.json`、`evidence_doctor.md` 并登记三者；`make evidence_status` 当前输出 `evidence registry check: fresh`。`log/board/evidence_doctor.md` 是包含 downsample 的历史单次 smoke doctor，不能作为本阶段 production-candidate 的当前 truth。

## Continue / Stop Decision

`EvidenceDecision = partial-production-candidate`。

默认下一阶段是 `010-production-integration-plan`：只写 PI1 production integration plan（生产接入计划），冻结 `io/src/image_depth.cpp` 的可接入范围、fallback、测试和板卡证据计划。继续到 PI2 会修改 production 源码；在用户明确授权 production integration loop 前，本轮停止在 PI1 计划边界。
