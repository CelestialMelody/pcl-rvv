# Phase 010 result: column-stride depth direct probe

## 当前结论

本阶段完成 `column-stride-depth-direct` 接入前诊断。该候选对固定窗口列使用 `vlse32.v` 直接跨 organized row stride 读取 depth `z`，只保留 weight（权重）暂存，相比 phase 000 去掉 depth staging。QEMU correctness（正确性）通过，反汇编可归属到 direct-depth RVV 指令，最新 Milkv-Jupiter 板卡 direct-depth case 仍呈正向：`1.01x`、`1.11x`、`1.26x`。

当前 EvidenceDecision（证据决策）：`partial-production-candidate`。含义是：建议进入 PI1 production integration plan（生产接入计划）做有界生产探针，但这仍不是 production-ready（生产就绪）或 adopted（已采纳）结论。进入 PI1 前需要用户确认，且当前仍不修改 `surface/include/pcl/surface/impl/bilateral_upsampling.hpp`。

## 计划回填

| action | 状态 | 命令 / 证据 | 结论 |
| --- | --- | --- | --- |
| candidate | done | `processColumnStrideDepthRVV` / `processDirectDepthCandidate` | 只在 `test-rvv/surface/bilateral_upsampling` 内实现，未触碰 production。 |
| correctness | done | `make -C test-rvv/surface/bilateral_upsampling run_test_compare` | QEMU std/RVV 两侧各 7 个测试通过。 |
| asm | done | `make -C test-rvv/surface/bilateral_upsampling dump_bench_rvv` | `build/asm/riscv/bench_bilateral_upsampling_rvv.full.asm` 中 direct-depth helper 附近可见 `vlse32.v`、`vmfeq.vv`、`vmerge.vvm`、`vfmul.vv`、`vfredusum.vs`。 |
| board | done | `make -C test-rvv/surface/bilateral_upsampling board_smoke` | direct-depth 三项为 near-threshold / positive，最大 case 达 `1.26x`。 |
| doctor | done-with-warnings | `doc/phases/010-column-stride-depth-direct-probe/evidence_manifest.json` -> `evidence-doctor.md` | `Errors=0`，Warnings 来自 diagnostic A/B 边界不同；结论降级为“可进 PI1”，不写成严格 production evidence。 |

## 证据分层

Correctness：`run_test_compare` 覆盖 depth / RGB 查表、全 NaN fallback、dense cloud、NaN holes、staged 候选、direct-depth 候选和窗口边界。std/RVV 两侧均为 7/7 passed。

QEMU path evidence（QEMU 路径证据）：QEMU 只证明 std/RVV 构建、fallback 和 correctness 路径可运行；没有使用 QEMU timing 作为性能证据。

Disassembly evidence（反汇编证据）：direct-depth 路径在 `processColumnStrideDepthRVV` / `processDirectDepthCandidate` 对应 bench 二进制中出现 `vlse32.v`、`vmfeq.vv`、`vmerge.vvm`、`vfmul.vv`、`vfredusum.vs`。其中 `vlse32.v` 说明 depth 走跨行 stride load，`vmfeq` / `vmerge` 用于 NaN lane 清零，`vfmul` / `vfredusum` 用于 `weight * z` 与 `weight` 规约。

Board performance（板卡性能）：`log/board/analyze_bench_compare.log` 来自 Milkv-Jupiter，5 iterations、2 warmup。下表只列 phase 010 direct-depth case；phase 000 staged case 仍是负向，不能混用为本候选结论。

| case | Std avg | RVV avg | speedup | bucket |
| --- | ---: | ---: | ---: | --- |
| `80x60 w3 dense` | 4.8879 ms | 4.8537 ms | 1.01x | neutral/weak-positive boundary，不能单独支撑 production |
| `120x90 w4 holes` | 19.0473 ms | 17.0909 ms | 1.11x | positive |
| `180x120 w5 dense` | 62.0909 ms | 49.2475 ms | 1.26x | positive |

Evidence Doctor（证据体检）：脚本读取 topic-local manifest 后报告 `Errors=0`、`Warnings=12`、`Suggestions=1`。十二个 Warning 都是 wrapper、timer boundary、mask、reduction 的 diagnostic 边界差异；这是有意的接入前 helper A/B，不可外推成同一 production boundary 内的严格性能结论。一个 Suggestion 是 `80x60 w3 dense` 仅 `1.01x`，贴近阈值；PI1 若继续，应把小图 near-threshold 写入风险，并优先冻结同边界 public entry bench。

## 诊断到生产错配审计回填

| question | result |
| --- | --- |
| evidence role | diagnostic |
| A/B boundary | test helper；baseline 为 `processScalar`，candidate 为 `processDirectDepthCandidate`。 |
| 当前决策问题 | RVV-vs-scalar，判断 direct-depth 形态是否值得进入 production integration loop。 |
| diagnostic 是否可外推到 production | partial。趋势只支持进入有界 PI1；小图最新只有 `1.01x`，不能直接声称 production 加速。 |
| comparison-boundary / baseline mismatch 风险 | yes，RVV 侧使用 strided depth load、NaN mask merge 和 vfredusum chunk reduction，边界不同。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | yes，本阶段有两项 positive 和一项 near-threshold，允许建议 PI1；小图 case 需要在 PI1 复核。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | yes。真实接入前必须补 production direct correctness、fallback、asm 和板卡 bench。 |

## 继续 / 停止决定

`continue_stop_decision`: stop_for_user_confirmation。

`stop_condition_hit`: 继续会进入 production integration loop 并修改 `surface/include/pcl/surface/impl/bilateral_upsampling.hpp`，需要用户确认。

`next_phase_default`: 建议进入 PI1 `column-stride-depth-direct-production-probe`，目标是冻结 `PointXYZRGB` / `PointXYZRGBA` layout、fallback 宏边界、public entry correctness、asm attribution 和板卡 bench；不建议接入 phase 000 的 staged-window-reduction 候选。
