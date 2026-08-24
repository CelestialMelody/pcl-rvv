# Phase 000 Result: current-state-and-diagnostic-scaffold

## 当前结论

本阶段完成了 `ONIGrabber::convertToXYZPointCloud` depth-only `PointXYZ` 的 production-shaped diagnostic（生产形态诊断）。结论是 `partial-production-candidate`：测试专用 RVV candidate（候选链路）在 Milkv-Jupiter 板卡 5-run repeated summary（重复板卡摘要）中稳定正向，median `1.17x`、min `1.13x`、max `1.20x`，Std/RVV checksum 一致。该证据仍不是 production direct（真实生产路径证据），不能证明真实 ONI replay public entry（公开入口）或 production dispatch（生产分流）已经成立。

## 计划动作回填

| action | status | command / evidence | conclusion |
| --- | --- | --- | --- |
| 写红灯测试 | done | `make run_test_compare` 首次失败，链接缺少 `fillXYZCloudCandidate` | TDD 红灯成立，测试能捕捉候选缺失。 |
| 实现候选 | done | `include/oni_grabber.h` | Std build 走 scalar reference；RVV build 走 RVV intrinsic。 |
| correctness | done | `make run_test_compare` | Std/RVV 各 2 个 gtest 通过。 |
| QEMU bench smoke | done | `make run_bench_rvv BENCH_ARGS="--case-filter xyz_depth_full_640x480 --iterations 2 --warmup-iterations 1"` | 输出 Dataset、Iterations、Total Time、checksum；QEMU timing 不作为性能结论。 |
| 反汇编归属 | done | `make check_oni_grabber_rvv_asm` | `vle16.v`、`vfcvt.f.xu.v`、`vfmul`、`vmseq`、`vsse32.v` 均出现。 |
| 板卡 smoke | done | `make run_board_oni_grabber_smoke` | 单次 `1.19x`，checksum 一致，只作为 smoke。 |
| 板卡 repeated | done | `make collect_board_oni_grabber_repeated` + `make generate_board_oni_grabber_repeated_summary` | 5-run summary positive：median `1.17x`，min `1.13x`，max `1.20x`。 |
| Evidence Doctor | done with warnings | `python3 ../../script/evidence_doctor.py --summary-md log/board/repeated_diagnostic/summary.md ...` | Errors=0，Warnings=2，Suggestions=2；结论降级为 summary-only diagnostic aid。 |

## Evidence paths

| evidence | path | role |
| --- | --- | --- |
| QEMU Std/RVV correctness logs | `log/qemu/run_test_std.log`, `log/qemu/run_test_rvv.log` | correctness（正确性） |
| QEMU RVV bench smoke | `log/qemu/run_bench_rvv.log` | log shape（日志形状），不是性能 |
| RVV asm summary | `build/asm/riscv/bench_oni_grabber_rvv.asm` | asm attribution（反汇编归属） |
| Board repeated raw compare logs | `log/board/repeated_diagnostic/analyze_bench_compare_*.log` | raw local evidence，默认不提交 |
| Board repeated summary | `log/board/repeated_diagnostic/summary.md` | board performance diagnostic summary |
| Evidence Doctor report | `log/board/repeated_diagnostic/evidence_doctor.md` | reviewer aid（审查辅助） |

## Diagnostic-to-production mismatch audit 回填

| question | result |
| --- | --- |
| evidence role | `production-shaped diagnostic`，不是 production direct。 |
| A/B boundary | test helper / production-shaped helper。Std 和 RVV 使用同一 synthetic depth frame、同一 checksum policy 和同一 bench wrapper。 |
| 当前决策问题 | `RVV-vs-scalar` 候选筛选。 |
| diagnostic 是否可外推到 production | 只能说明 depth projection helper 值得做有界生产探针；不能说明真实 ONI replay public entry 受益。 |
| comparison-boundary / baseline mismatch 风险 | 仍存在：production public entry 包含 ONI reader、device wrapper、depth buffer resize 和 replay 调度。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 本阶段不是弱 / 负 / 中性 / 不稳定；若后续 production direct 变弱，应停在 PI5 用户检查点。 |
| clean adoption 是否需要同一 production boundary 内证据 | 需要。必须先完成 PI1-PI5，包括 production direct correctness、fallback、asm、board repeated 和用户确认。 |

## Evidence Doctor 解释

Evidence Doctor（证据体检）对 Markdown summary 的轻量检查结果为 `Errors=0, Warnings=2, Suggestions=2`。

| finding | 处理 |
| --- | --- |
| `summary_only_metadata_missing` | 当前没有 JSON manifest，不能写成完整 Evidence Doctor 通过；只作为 reviewer aid。 |
| `missing_run_contract` | summary 文本包含 input logs 的 iterations，但缺少结构化 warmup / run contract 字段；result 中补写本轮命令为 `iterations=10, warmup=2, runs=5`。 |
| `environment_metadata_missing` | 当前只记录 device=Milkv-Jupiter，缺 taskset/governor/freq/temperature；不影响 positive diagnostic，但 production integration 必须补更完整 manifest。 |
| `binary_identity_missing` | 当前没有 binary hash；若进入 production integration，PI4 应补 binary identity 或等价 build label。 |

## Optimization matrix update

| candidate family | decision | evidence | unblocked next action |
| --- | --- | --- | --- |
| depth-projection-rvv | `partial-production-candidate` | correctness pass；QEMU smoke；asm pass；board repeated median `1.17x` | 需要用户确认后进入 PI1 production integration plan。 |
| rgb-rgba-ir-extension | `deferred` | OpenNI2 历史 RGB/IR weak / unstable；当前 phase 未覆盖 | 只有 depth production probe 成立后才重新审计。 |
| full ONI replay public-entry bench | `deferred` | 当前没有真实 ONI 文件 / replay profile | 用户提供 ONI replay 场景或 profile 后另开 public-entry phase。 |

## Continue / Stop decision

`continue_stop_decision = turn_stop_deferred with stop_condition_hit`。停止原因不是板卡或工具问题；板卡已可用且本阶段证据已跑完。停止原因是继续下一步会修改 production 文件并进入 production integration loop（生产接入闭环），而当前用户 prompt 授权了 topic 优化但没有明确确认 PI1-PI5 生产接入和 PI5 后的人工检查边界。默认下一动作是：用户确认进入 production integration loop 后，创建 PI1 production integration plan，冻结只覆盖 `convertToXYZPointCloud` / `PointXYZ` / depth-only / `Scalar=float` / organized contiguous depth frame 的范围。

## 未覆盖范围

RGB/RGBA、IR、真实 ONI 文件吞吐、depth/image 尺寸不一致、replay reader 调度、production fallback、`HAVE_OPENNI` 构建边界和 production direct 证据均未覆盖。当前诊断 positive 不能外推成这些范围已完成。
