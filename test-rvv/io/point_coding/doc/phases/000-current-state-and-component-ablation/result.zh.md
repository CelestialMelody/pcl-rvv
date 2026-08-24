# Phase 000：当前状态与 point coding 组件消融结果

## 执行范围

本阶段按 `plan.zh.md` 完成 `io/include/pcl/compression/point_coding.h` 的 component ablation（组件消融）。实际改动只在 `test-rvv/io/point_coding`，没有修改 production（生产源码），没有创建 `doc-rvv/io/point_coding-RVV.zh.md`。

validated_scope（已验证范围）：

- `PointXYZ` AoS（结构数组）布局。
- encode source-indexed leaf（leaf 内 indices 指定输入点）。
- decode contiguous output segment（连续输出片段）。
- test helper A/B boundary（测试 helper 对比边界）。

unvalidated_scope（未验证范围）：

- 完整 octree traversal（八叉树遍历）、entropy context（熵编码上下文）、真实 leaf size 分布和 public entry。
- 泛型 `PointT`、其它字段布局、production fallback（生产回退路径）和 production direct（真实生产路径证据）。

## 计划动作回填

| 动作 | 状态 | 命令 / 证据 | 结论 |
| --- | --- | --- | --- |
| RED correctness | done | `make run_test_compare` 在聚合头缺失时失败。 | 测试支撑入口由失败合同驱动创建。 |
| 新建 test support | done | `include/point_coding.h`、`include/impl/point_coding_support.hpp`、`src/test_point_coding.cpp`。 | Std / RVV helper 分层清楚。 |
| GREEN correctness | done | `make run_test_compare`。 | Std / RVV 各 3 个 test pass。 |
| bench / QEMU smoke | done | `make run_qemu_bench_smoke`。 | RVV bench 可运行；QEMU timing 不作为性能。 |
| asm attribution（反汇编归属） | done | `make dump_bench_rvv`。 | 可见 `vluxseg3ei32.v`、`vlse8.v`、`vsse32.v`。 |
| board correctness / bench | done | `make run_board_test && make run_board_bench_compare && make fetch_board_logs`。 | 板卡可用，单次 test / bench 路径闭合。 |
| repeated board | done | `make collect_board_repeated POINT_CODING_REPEATED_RUNS=5`。 | 生成 `log/board/repeated_phase000/summary.md`。 |
| Evidence Doctor | done | `make run_board_repeated_evidence_doctor`。 | Errors=0，Warnings=6，Suggestions=0。 |
| topic-local docs | done | README、evaluation、testing、bench、optimization、code map、roadmap、matrix。 | 可恢复文档套件已补齐。 |

## 性能结果

| case | median | min | max | decision bucket |
| --- | ---: | ---: | ---: | --- |
| `encode_indexed_1024` | 2.11x | 2.04x | 2.15x | positive |
| `encode_indexed_4096` | 1.63x | 1.50x | 1.76x | positive with long-tail warning |
| `encode_indexed_16384` | 1.27x | 1.23x | 1.32x | positive but group outlier |
| `decode_contiguous_1024` | 1.27x | 0.80x | 1.39x | unstable / weak-positive |
| `decode_contiguous_4096` | 1.19x | 1.16x | 1.22x | weak-positive |
| `decode_contiguous_16384` | 1.17x | 1.02x | 1.17x | weak-positive with long-tail warning |

板卡复跑预算为 5-run，本阶段不无限复跑。encode 三个规模均正向，但 Evidence Doctor 指出规模敏感；decode 正向较弱，小规模存在退化。

## Evidence Doctor 处理

`log/board/repeated_phase000/evidence_doctor.md` 报告 `Errors=0，Warnings=6，Suggestions=0`。

| finding | 处理动作 | 对结论影响 |
| --- | --- | --- |
| `decode_contiguous_1024` degradation frequency | 保留 1/5 退化事实，不剔除异常。 | 小规模 decode 降级为 unstable / weak-positive。 |
| `decode_contiguous_1024` long-tail | 保留 min / median / max。 | 不能 clean-adopt production。 |
| `decode_contiguous_16384` long-tail | 保留长尾说明。 | decode 大规模只写 weak-positive。 |
| `encode_indexed_4096` long-tail | 保留长尾说明。 | encode 4096 仍 positive，但需要 leaf-size sweep。 |
| `encode_indexed_1024` group outlier | 不外推到所有 leaf。 | 下一 phase 要做 leaf-size sensitivity。 |
| `encode_indexed_16384` group outlier | 不外推到所有 leaf。 | 下一 phase 要做 leaf-size sensitivity。 |

## 诊断到生产错配审计回填

| question | answer |
| --- | --- |
| evidence role | component_ablation / diagnostic。 |
| A/B boundary | test helper。 |
| 当前决策问题 | RVV-vs-scalar component viability。 |
| diagnostic 是否可外推到 production | no；完整 production 成本边界缺 traversal、entropy、真实 leaf size 和 fallback。 |
| comparison-boundary / baseline mismatch 风险 | helper A/B 低风险；生产外推高风险。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | Phase 000 不允许直接 production probe；需要下一 phase 先做 production-shaped boundary scout。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | yes；当前没有 production boundary。 |

## 实现取舍结果

完整 f32 RVV encode 量化公式没有保留为最终候选。原因是 production 标量链路使用 float coordinate + double reference 的 double 运算后截断；f32 RVV 在边界附近可能差 1。当前 encode candidate 改为 RVV indexed gather 后回到标量 same-chain quantize（同构量化），这是组件诊断边界，不是 production-ready 形态。

decode candidate 采用 `vlse8` 跨步加载 diff byte，扩展后用 `vsse32` 写回 AoS 字段。它的语义边界更直接，但小规模波动阻止生产结论。

## Doc Suite Role Inventory

| role | 状态 | 路径 / 证据 |
| --- | --- | --- |
| topic_navigation | standalone | `README.zh.md` |
| testing_overview | standalone | `doc/testing-overview.zh.md` |
| correctness_tests | standalone | `doc/correctness-tests.zh.md` |
| benchmark_and_evidence | standalone | `doc/benchmark-and-evidence.zh.md` |
| optimization_evidence | standalone | `doc/optimization-evidence.zh.md` |
| optimization_roadmap | standalone | `doc/optimization-roadmap.zh.md` |
| test_support_code_map | standalone | `doc/test-support-code-map.zh.md` |
| phase_index | standalone | `doc/phases/README.zh.md` |
| evaluation_diagnostic | standalone | `doc/point_coding-evaluation.zh.md` |
| production_topic_doc | not_applicable with evidence | 没有 adopted production behavior，也没有 PI5 通过后的生产补丁。 |

## Structure Parity 审计

| area | current shape scan | quality bar | decision | blocker / evidence | next action |
| --- | --- | --- | --- | --- | --- |
| test / bench source layout | 使用 `src/`。 | 配置解析 source layout。 | adopted | 当前符合。 | 无。 |
| aggregator / internal helpers | 使用 `include/` 和 `include/impl/`。 | `test_support` 默认结构。 | adopted | 当前符合。 | helper 膨胀时再拆。 |
| script / manifest | topic-local wrapper 存在。 | Evidence Doctor 需要 manifest。 | adopted | doctor 已运行。 | 可选 registry 接入。 |
| target granularity | aggregate、QEMU smoke、board repeated、doctor target 存在。 | doc-suite target audit。 | adopted | `doc/testing-overview.zh.md` 已记录。 | 新增 production-shaped smoke 时拆 alias。 |
| topic-local docs | README、evaluation、testing、bench、optimization、code map、roadmap、phase index 已补齐。 | doc-suite quality bar。 | adopted | 当前文件存在。 | 下一 phase 更新即可。 |
| long-term `doc-rvv` | 不存在。 | 只适用于 adopted production。 | not_applicable with evidence | 当前未改 production。 | PI5 通过并用户确认后再创建。 |
| legacy compatibility | 无旧入口。 | 默认不保留 pointer / alias。 | not_applicable with evidence | 新 topic。 | 无。 |

## Evidence registry 状态

当前没有 `log/evidence_registry.json`。本阶段用 `git status --short --untracked-files=all -- test-rvv/io/point_coding` 和 `git ls-files --others --exclude-standard -- test-rvv/io/point_coding` 做人工 artifact tracking（产物跟踪）检查。生成日志在 `log/` 下默认 ignored-local；topic 文档引用这些路径作为当前证据来源，但不默认提交 raw logs。

## Continue / Stop Decision

`continue_stop_decision`：继续当前 topic，但不进入 production。Phase 000 已完成，未命中板卡不可用、工具失败、dirty isolation 不安全或证据矛盾等停止条件。仍有授权范围内的下一动作：`010-production-shaped-boundary-scout`。

`next_phase_default`：创建 `010-production-shaped-boundary-scout/plan.zh.md`，优先回答两个问题：

1. 真实 leaf size / octree-shaped context 是否会让 Phase 000 的局部收益消失。
2. encode 量化能否在保持 double 语义的前提下形成更完整 RVV 候选。
