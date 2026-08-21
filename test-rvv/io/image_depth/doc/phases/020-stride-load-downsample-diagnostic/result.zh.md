# Phase 020 结果：stride-load downsample diagnostic

## 执行范围

本阶段只修改 `test-rvv/io/image_depth` 下的测试资产和 topic-local 文档，没有修改 `io/src/image_depth.cpp`。实际覆盖的是 production-shaped diagnostic（生产形态诊断）：`fillDepthImage()` / `fillDisparityImage()` 的测试专用 helper 在整数倍 downsample（下采样）且 `xStep > 1` 时走 `vlse16`（跨步加载 16-bit 元素）RVV candidate。

| 范围 | 状态 | 说明 |
| --- | --- | --- |
| depth downsample `640x480 -> 320x240` | done | RVV build 选择 `CandidatePath::DownsampleRvv`，Std build 仍选择 scalar fallback。 |
| disparity downsample `640x480 -> 320x240` | done | 与 depth 使用同一整数倍 downsample gate；disparity 保持 `constant = focal_length * baseline * 1000 / xStep` 语义。 |
| 非整数 downsample / upsample | done as fallback boundary | 当前 selector 不把这些入口纳入 RVV candidate；继续由标量参考链路覆盖。 |
| production direct | not_applicable | 本阶段没有真实生产分流、公开入口测试或 production asm 证据。 |

## 计划动作回填

| 计划动作 | 状态 | 命令 / 证据路径 | 结论 |
| --- | --- | --- | --- |
| RED path-selection test | done | `make run_test_rvv` 首次失败，缺少 `CandidatePath` / selector | TDD（测试驱动开发）红灯有效，失败指向“RVV build 不能选择 downsample path”。 |
| GREEN downsample helper | done | `include/image_depth.h`、`src/test_image_depth.cpp` | 新增 `CandidatePath::DownsampleRvv`、整数倍 downsample selector 和 depth/disparity `vlse16` helper。 |
| correctness compare | done | `make run_test_compare` | Std/RVV 两个 build 均通过 4 个 gtest；新增测试证明 Std fallback 与 RVV downsample path-selection 分离。 |
| QEMU bench smoke | done | `make run_bench_rvv BENCH_ARGS="--case-filter all --iterations 2 --warmup-iterations 1"`、`make dump_bench_rvv` | 只证明 RVV bench 可运行和日志形状，不作为性能结论。 |
| asm attribution | done | `build/asm/riscv/bench_image_depth_rvv.asm` | asm 中可见 `vlse16.v`、`vfmul.vf`、`vfrdiv.vf` 和 masked `vse32.v`，说明 downsample helper 命中跨步加载 RVV 指令。 |
| board repeated | done | `make collect_board_downsample_repeated`、`make run_board_downsample_evidence_doctor` | Milkv-Jupiter 5-run repeated summary 对 depth/disparity downsample 均为 positive，但存在 long-tail Warning。 |
| stale-log 修正 | done | `Makefile` 的 `collect_board_downsample_repeated` 在每轮 `run_board_bench_compare` 后执行 `fetch_board_logs` | 避免复制旧的本地 `log/board/analyze_bench_compare.log`；当前 summary 来自本阶段 5 个 run-labelled compare log。 |

## 诊断证据链

| 证据层 | 当前证据 | 证明什么 | 不能证明什么 |
| --- | --- | --- | --- |
| correctness | `make run_test_compare` 通过 4 个测试 | 测试专用 helper 的 scalar-vs-candidate 对拍覆盖 contiguous、padding、downsample fallback 和 RVV path selection。 | 不能证明真实 `DepthImage` 公开入口已经分流到 RVV。 |
| QEMU path | `run_bench_rvv` smoke 可运行 | RVV build 的 bench wrapper 可运行，downsample case 日志可解析。 | QEMU timing 不能作为性能结论。 |
| asm attribution | `build/asm/riscv/bench_image_depth_rvv.asm` | RVV candidate 中存在 `vlse16` 跨步加载，并包含转换、乘法 / 除法和 masked store。 | 反汇编归属于 test-rvv bench binary，不是 production 符号。 |
| board performance | `log/board/repeated_downsample/summary.md` | Milkv-Jupiter 上 downsample production-shaped helper 相对 scalar reference 正向。 | 不能外推到 production direct、非整数 downsample、OpenNI legacy 或 `fillDepthImageRaw()`。 |
| Evidence Doctor | `log/board/repeated_downsample/evidence_doctor.md` | Errors=0，说明当前 summary / manifest 没有阻塞错误。 | Warnings=2，长尾说明不能只用单一均值，也不能写成 clean production evidence。 |

## repeated board 结果

| case | runs | median | min | max | decision bucket | 说明 |
| --- | ---: | ---: | ---: | ---: | --- | --- |
| `depth_downsample_640x480_to_320x240` | 5 | 1.17x | 1.03x | 1.24x | positive with warning | median 高于 1.10x，min 高于 1.02x；max/min=1.21，保留长尾 warning。 |
| `disparity_downsample_640x480_to_320x240` | 5 | 1.38x | 1.13x | 1.66x | positive with warning | 收益更明显；max/min=1.47，后续 production probe 需要扩大 runs 或补 per-iteration trace。 |

## Evidence Doctor 处理

`log/board/repeated_downsample/evidence_doctor.md` 输出 Errors=0，Warnings=2，Suggestions=0。两个 Warning 均为 `long_tail_or_variance`（长尾或方差偏大）：

- depth downsample：min=1.03x、median=1.17x、max=1.24x。
- disparity downsample：min=1.13x、median=1.38x、max=1.66x。

处理动作：不剔除任何 run；阶段结论保留 min/median/max，并把当前证据角色限定为 production-shaped diagnostic。若后续进入 production direct（真实生产路径证据）或把 downsample 纳入 PI2 production patch，PI4 应扩大 run count 或补 taskset/governor/freq/temperature/binary hash 等 metadata，再由 Evidence Doctor 复核。

## diagnostic-to-production mismatch audit 回填

| question | result |
| --- | --- |
| evidence role | `production_shaped_diagnostic`，不是 production-public 或 production-detail。 |
| A/B boundary | baseline 是测试专用 scalar reference，candidate 是测试专用 RVV helper；两侧使用同一 bench wrapper、checksum policy 和 timer boundary。 |
| 当前决策问题 | `vlse16` downsample 是否值得保留为后续 production candidate。 |
| diagnostic 是否可外推到 production | 可以支撑“值得纳入有界 production probe 候选”；不能直接支撑 adoption（采纳）。 |
| comparison-boundary / baseline mismatch 风险 | helper 不含真实 `DepthImage` dispatch、异常路径和公开入口调用成本；production patch 仍需单独 gate、fallback、asm 和板卡证据。 |
| 弱 / 负 / 中性 / 不稳定时 bounded production probe | 当前为 positive with warning，因此允许作为候选；若 production direct 降为 weak / unstable，必须在 PI5 暂停等用户判断。 |
| clean adoption 是否需要 production boundary 证据 | 需要。本阶段不能 clean adopt，也不能创建 `doc-rvv/io/image_depth-RVV.zh.md`。 |

## Optimization matrix 更新

- `downsample depth vlse16 RVV`：`planned -> partial-production-candidate`。Correctness、QEMU smoke、asm、board repeated 和 Evidence Doctor 已闭合；Evidence Doctor 有长尾 Warning，production direct 缺口未闭合。
- `downsample disparity vlse16 RVV`：`planned -> partial-production-candidate`。收益高于 depth，但长尾更明显；后续 production probe 需要扩大 runs 或补环境 metadata。
- `contiguous depth/disparity RVV`：保持 phase 000 的 `partial-production-candidate`；本阶段没有改变其证据。
- `OpenNI legacy parity`、`fillDepthImageRaw`：仍为 deferred / not_applicable，不被本阶段覆盖。

## Evidence freshness 与 registry

当前阶段生成并引用：

- `log/board/repeated_downsample/summary.md`
- `log/board/repeated_downsample/evidence_manifest.json`
- `log/board/repeated_downsample/evidence_doctor.md`

这些摘要证据已通过 `make record_board_downsample_evidence_state` 登记到 `log/evidence_registry.json`，随后 `make evidence_status` 输出 fresh。raw `run_bench_*.log` 和 `analyze_bench_compare_*.log` 只作为本机原始输入，不进入 summary-only 提交候选。

## Continue / Stop Decision

`EvidenceDecision = partial-production-candidate` for downsample stride-load RVV。

当前没有命中“板卡不可用”或“证据 Error”停止条件。仍然存在一个真实 stop condition：继续到 PI2 会修改 `io/src/image_depth.cpp`，需要用户明确授权 production integration loop（生产接入闭环）。默认下一步是等待用户授权后修订 PI1 / 进入 PI2，把 contiguous 和 downsample 的生产候选边界一并冻结；若用户不授权 production，可继续在 test-rvv 内做 OpenNI legacy parity 或 `fillDepthImageRaw` 低优先级诊断。
