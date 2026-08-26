# PFHRGB Testing Overview

本文负责 PFHRGB topic 的测试入口总览和证据边界。每个 TEST（测试用例）的断言细节见
`doc/correctness-tests.zh.md`；bench（性能测试）case、board（板卡）和 Evidence Doctor（证据体检）
细节见 `doc/benchmark-and-evidence.zh.md`。

## 运行入口分类

| 类别 | 当前入口 | 证明范围 | 不证明范围 |
| --- | --- | --- | --- |
| correctness aggregate（正确性汇总入口） | `make -C test-rvv/features/pfhrgb run_test_compare` | Std/RVV 两侧 gtest 全部通过，验证 reference、production exact path（精确点型生产路径）、fallback、candidate、public-shaped wrapper 和 reusable workspace 输出一致。 | 不证明真实性能、泛型 RGB traits（RGB 字段特征）或所有 fallback 原因。 |
| correctness 单侧入口 | `run_test_std`、`run_test_rvv` | 分别验证标量构建和 RVV 构建可运行。 | 单侧通过不能构成 Std/RVV 数值对拍。 |
| diagnostic bench | `run_bench_std`、`run_bench_rvv` 配合 `--case-filter` | 隔离 component、public-like、public-shaped candidate 和 reuse case。 | QEMU timing（QEMU 计时）不代表性能。 |
| QEMU smoke（QEMU 小型验证） | `run_bench_* BENCH_ARGS="--side 8 --k 8 --iterations 1 --warmup 0 --case-filter public_pfhrgb_k_with_candidate_reuse"` | 证明新增 bench label 可运行、checksum 可解析。 | 不证明板卡性能。 |
| board repeated（板卡重复采集） | `REPEATED_BOARD_RUNS=5 board_repeated evidence_doctor_repeated` | 生成 5-run board summary、manifest 和 Doctor；接入后 `public_pfhrgb_k` 是 production-public（生产公开入口）证据。 | 不证明其它点型、radius search、真实数据集或其它目标硬件。 |
| doctor / registry（证据体检和登记） | `evidence_doctor_repeated`、`log/evidence_registry.json` | 暴露 Errors / Warnings / Suggestions，登记 summary-only 证据。 | 不替代 reviewer 对 production boundary（生产边界）的判断。 |

## 输入数据和覆盖范围

当前测试使用 synthetic dense finite cloud（合成、稠密、有限点云），点类型为
`pcl::PointXYZRGBNormal`，输出为 `pcl::PFHRGBSignature250`，`Scalar=float`，默认 `nr_split=5`。
row source policy（行来源策略）覆盖 fixed neighborhood indices（固定邻域索引）和 public KSearch
neighborhood（公开 K 近邻搜索邻域）。当前 production 证据覆盖 exact-gated
`PointXYZRGBNormal -> PointXYZRGBNormal -> PFHRGBSignature250`、`nr_split=5` 和 KSearch public boundary。
未覆盖泛型 RGB traits、`Scalar=double`、真实数据集、radius search、OMP 分支或完整 fallback matrix。

## Target 粒度审计

| target 类别 | decision | evidence / path | next action |
| --- | --- | --- | --- |
| correctness aggregate | adopted | `Makefile` 的 `run_test_compare` 覆盖 Std/RVV 两侧 6 个 gtest。 | 后续扩展点型或 fallback 原因时再补更细 target。 |
| correctness aliases | adopted for current scope | `run_test_std`、`run_test_rvv` 可单独运行；当前 gtest 数量可由 TEST 名定位。 | 如果继续 point-type expansion（点类型扩展），再拆更细 target。 |
| bench diagnostic aliases | adopted | `src/bench_pfhrgb.cpp` 通过 `--case-filter` 隔离五个 case；`public_pfhrgb_k` 当前已重标为 production-public。 | 若继续新 implementation family（实现族）比较，再补 RVV-vs-RVV detail A/B。 |
| QEMU smoke aliases | adopted | QEMU 只用于 correctness、构建和 log-shape。 | 不把 QEMU timing 写入性能结论。 |
| board smoke aliases | adopted | `check_board_ssh` 可验证连通性；`board_repeated` 负责性能重复采集。 | 后续扩展新点型或真实数据集时复用。 |
| board repeated aliases | adopted | `board_repeated` 每轮生成 Std/RVV bench 和 compare summary。 | 生产补丁后重跑同 target。 |
| doctor / registry aliases | adopted | `script/generate_pfhrgb_evidence_manifest.py`、`evidence_doctor_repeated`、`log/evidence_registry.json`。 | 若新增 production metadata 或新 case，更新 manifest 字段。 |
| historical probe guarded aliases | not_applicable with evidence | 当前没有历史 production probe 或 rollback target。 | PI5 若需要回滚，再新增 guarded target。 |

## 测试流程

默认流程是先跑 `run_test_compare`，再跑 `dump_bench_rvv` 检查 RVV 指令存在，随后只在板卡上运行
`REPEATED_BOARD_RUNS=5 board_repeated evidence_doctor_repeated`。若只是验证 bench label 或输出格式，可以用
QEMU smoke，但必须把结果写成 correctness / log-shape evidence（日志形状证据），不能写成性能证据。

## 当前结论边界

当前 board repeated 支持 `public_pfhrgb_k` 作为 positive production-public evidence，B/A 为
`1.28, 1.27, 1.28, 1.27, 1.27`，median `1.27x`，0/5 低于 1。`public_pfhrgb_k_with_candidate`
和 `public_pfhrgb_k_with_candidate_reuse` 只保留为 production-shaped diagnostic（生产形态诊断）背景。
Evidence Doctor Error 把 component/helper 级收益降级，因此当前采纳依据必须是接入后的 public boundary
（公开入口边界），不是 helper-only speedup。
