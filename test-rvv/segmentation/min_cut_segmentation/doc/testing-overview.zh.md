# min_cut_segmentation 测试总览

## 本文职责

本文说明 `min_cut_segmentation` RVV topic-local 测试入口和证据边界。它不解释每个 `TEST` 的断言细节，也不把 QEMU（仿真器）计时当成性能证据。

## 文档阅读路径

| 问题 | 主入口 |
| --- | --- |
| 当前 topic 结论 | `README.zh.md` |
| 函数级评估和 no-production 判断 | `doc/min_cut_segmentation-evaluation.zh.md` |
| correctness（正确性）测试细节 | `doc/correctness-tests.zh.md` |
| bench、summary、manifest、Evidence Doctor（证据体检） | `doc/benchmark-and-evidence.zh.md` |
| 候选取舍 | `doc/optimization-evidence.zh.md`、`doc/optimization-roadmap.zh.md` |
| 测试支撑代码定位 | `doc/test-support-code-map.zh.md` |
| phase 恢复 | `doc/phases/README.zh.md` |

## 测试类型定义

| 类型 | 当前入口 | 证明范围 |
| --- | --- | --- |
| correctness aggregate（正确性汇总入口） | `make run_test_compare` | Std / RVV helper 在 QEMU 上对拍通过 |
| correctness aliases（正确性细分入口） | `make run_test_std`、`make run_test_rvv` | 分别验证非 RVV 宏和 RVV 宏构建 |
| diagnostic bench（诊断性能测试） | `make run_bench_std`、`make run_bench_rvv` | 生成 bench 日志；本地 / QEMU 不作为性能结论 |
| QEMU smoke（QEMU 小型验证） | `make run_test_compare`、`make dump_bench_rvv` | 构建、路径和反汇编归属 |
| board repeated（板卡重复采集） | `make run_board_min_cut_repeated`、`make run_board_min_cut_buildgraph_repeated` | 目标硬件性能证据 |
| doctor / manifest | `make run_board_evidence_doctor`、`make run_board_buildgraph_evidence_doctor` | 检查 repeated 证据形态和退化信号 |
| historical probe（历史探针） | not_applicable with evidence | 本 topic 未保留历史生产探针 target |

## 运行入口分类

| Make target / script | 分类 | 默认参数 / 过滤 | 不能证明 |
| --- | --- | --- | --- |
| `run_test_compare` | correctness aggregate | 运行 Std 和 RVV gtest | production public dispatch |
| `dump_bench_rvv` | asm attribution（反汇编归属） | RVV bench binary | 性能收益或 public hit |
| `run_board_min_cut_repeated` | component repeated board | `unary_min_distance`、`binary_exp_weight` | KNN / graph / max-flow 后收益 |
| `run_board_min_cut_buildgraph_repeated` | production-shaped repeated board | `buildgraph_potential_batch` | public `extract()` 和 max-flow |
| `script/generate_min_cut_board_evidence_manifest.py` | manifest / summary | 解析 repeated `analyze_bench_compare.log` | raw log 脱敏或 registry |

## Target 粒度审计

| target 类别 | 当前状态 | 证明什么 | 缺口 / 决策 |
| --- | --- | --- | --- |
| correctness aggregate | adopted | 3 个测试在 Std / RVV 两侧通过 | 无当前未阻塞缺口 |
| correctness aliases | adopted | 可单独跑 Std 或 RVV 编译路径 | 不需要按 gtest filter 再拆，当前 TEST 数量少 |
| bench diagnostic aliases | adopted | `--case-filter` 可隔离 component 和 buildGraph-shaped case | QEMU compare 默认受 guard 限制 |
| QEMU smoke aliases | adopted | correctness 和 asm | 不作为性能结论 |
| board smoke aliases | merged into repeated targets | repeated target 内部完成部署和运行 | 单次 smoke 不是当前提交证据 |
| board repeated aliases | adopted | 两个 repeated 目录分别承载 Phase 000 / 010 | 不提交 raw logs |
| doctor / registry aliases | adopted for doctor, registry not_applicable | manifest + Evidence Doctor 已生成；未接公共 registry | registry 缺失不影响 no-production closeout |
| historical probe guarded aliases | not_applicable with evidence | 无生产 patch、无历史探针 | 不需要 guard target |

## 测试流程

推荐顺序：

1. `make -C test-rvv/segmentation/min_cut_segmentation run_test_compare`
2. `make -C test-rvv/segmentation/min_cut_segmentation dump_bench_rvv`
3. `make -C test-rvv/segmentation/min_cut_segmentation run_board_min_cut_repeated`
4. `make -C test-rvv/segmentation/min_cut_segmentation run_board_min_cut_buildgraph_repeated`

## 输入数据总览

当前 fixture 使用 dense synthetic `PointXYZ` cloud，`float` xyz AoS（结构数组）布局，完整 `indices`。Phase 000 的 row source（行来源）为 input-indexed point rows 和 edge list source/target pairs；Phase 010 的 row source 为 full input rows plus KNN edge rows。

## 覆盖矩阵

| 路径 | QEMU correctness | asm | board repeated | production direct |
| --- | --- | --- | --- | --- |
| unary component | yes | yes | positive | no |
| binary component | yes | yes | positive | no |
| buildGraph-shaped helper | yes | yes | neutral + doctor error | no |
| public `extract()` | no | no | no | no |

## 可提交证据和默认排除项

提交候选只包含 topic-local 源码、脚本和文档。`log/` summary / manifest / doctor 是可引用证据，但默认 `topic-only` 提交不包含它们；raw run logs 和 `build/` 二进制不提交。

## 当前结论边界

当前证据支持 no-production closeout（不接入生产收尾）：component 有上界收益，但 buildGraph-shaped repeated 为 neutral 且 Evidence Doctor 报 Error。该结论不需要继续到 production integration loop（生产接入闭环）。
