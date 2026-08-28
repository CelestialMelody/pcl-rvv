# Phase 000: sphere select/getDistances 诊断计划

## 阶段意图和边界

本阶段针对 `sample_consensus/include/pcl/sample_consensus/impl/sac_model_sphere.hpp` 的基础球模型距离入口建立独立 topic。当前 production（生产源码）已经有 `countWithinDistanceRVV`，但 `selectWithinDistance` 和 `getDistancesToModel` 仍直接走标量循环。本阶段先补 production-shaped diagnostic（生产形态诊断，测试专用代码按真实入口的数据形状运行）和 board bench（板卡性能测试），不修改 production 头文件，不把诊断结果写成 adopted production behavior（已采用生产行为）。

阶段范围只覆盖 `SampleConsensusModelSphere<PointT>` 的 direct indexed `indices_` 入口、registered single-float x/y/z 点型、`Eigen::VectorXf` float 系数、`double threshold` 转 float 后的球壳双边界判断。阶段不覆盖 circle、normal-sphere、SAC 后处理、`Scalar=double`、自定义非标准 layout 或 production dispatch（生产分流）新增。

## 当前状态清单

| area | current state | path |
| --- | --- | --- |
| production count | `countWithinDistance` 在 `__RVV10__` 且 `RVVXYZFloatLayout<PointT>` 成立时分流到 `countWithinDistanceRVV`。 | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_sphere.hpp` |
| production select/getDistances | 两个公开入口直接执行标量循环；没有 Std/RVV helper 分层。 | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_sphere.hpp` |
| historical tests | `quadric_models` 覆盖 sphere RANSAC、count smoke/perf 和 projectPoints；输出日志混在历史 quadric topic。 | `test-rvv/sample_consensus/quadric_models/` |
| independent topic assets | 本阶段新建独立 `sac_model_sphere` topic，用于隔离 sphere select/getDistances 诊断。 | `test-rvv/sample_consensus/sac_model_sphere/` |
| board | 用户说明板卡可用，主要需要把 `SSH_AUTH_SOCK` 注入当前命令环境。 | prompt + `test-rvv/mk/rvv-topic.mk` |

## validated_scope / unvalidated_scope

| scope type | 内容 |
| --- | --- |
| validated_scope | 本阶段准备验证 `PointXYZ` 和 `PointXYZI` 的 direct indexed `indices_`，覆盖公开 `selectWithinDistance`、`countWithinDistance`、`getDistancesToModel` 与测试专用 RVV 候选输出一致性。 |
| unvalidated_scope | 其它 PointXYZ-like 点型、非 registered single-float xyz layout、超大 cloud 的 32-bit byte offset gate、circle/normal-sphere 公式、production dispatch 新增和 `Scalar=double`。 |
| point_type_expansion_queue | Phase 010 可在本阶段证据正向后扩展到 `PointXYZRGB` / `PointXYZRGBA` 或自定义 registered xyz 点型，并补 fallback、asm、board 和 Evidence Doctor（证据体检）。 |
| phase_closeout_boundary | 本阶段只能关闭 sphere direct indexed、代表点型和测试专用候选的诊断矩阵条目；不能关闭生产接入或泛型模板入口。 |

## diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | production-shaped diagnostic；测试包含真实 production header，并在 test-only subclass（测试专用派生类）中复刻 select/getDistances 的 RVV 候选。 |
| A/B boundary | public overload + test helper；bench 计时边界只包含目标入口或测试专用候选函数调用，不包含点云、indices、系数构造。 |
| 当前决策问题 | RVV-vs-scalar 的候选筛选，以及是否值得进入后续 production integration loop（生产接入闭环）。 |
| diagnostic 是否可外推到 production | 不能直接外推；它只能证明同一输入形状下候选公式、load 和输出合同可行。production 需要后续 PI1-PI5。 |
| comparison-boundary / baseline mismatch 风险 | 有；Std/RVV 是两个构建，测试专用候选不是 production dispatch。bench 必须保留相同输入、indices、系数、threshold、warmup 和 iteration。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 允许条件是 correctness、边界 case 和 asm 归属通过，且弱/负向结果可归因到测试候选输出边界而非生产语义风险；否则只保留 diagnostic。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 是。若后续生产接入选择新 helper 族，必须在 production detail helper 内补同边界 RVV-vs-RVV A/B，不能用本阶段 public Std/RVV 或测试 helper 直接 clean-adopt。 |

## 实现和测试动作

| action | files | command / evidence | completion |
| --- | --- | --- | --- |
| 新增独立 topic Makefile 和 board.mk | `Makefile`、`board.mk` | `make -C test-rvv/sample_consensus/sac_model_sphere run_test_compare` | Std/RVV 两个构建可运行。 |
| 新增 correctness harness | `src/test_sac_model_sphere.cpp` | `run_test_compare` | public entry、标量参考和测试专用 RVV 候选在边界点、乱序 indices、`PointXYZI` 上一致。 |
| 新增 bench harness | `src/bench_sac_model_sphere.cpp` | board `run_board_bench_compare` | 输出 Dataset、Iterations、Warmup Iterations、Checksum 和 select/count/getDistances/candidate 行。 |
| QEMU correctness | topic Makefile | `run_test_compare` | QEMU 只作为 correctness 和日志形状证据，不写性能结论。 |
| asm attribution | topic Makefile | `dump_bench_rvv` | RVV bench 二进制能看到候选 helper 或 count helper 的 RVV 指令。 |
| board evidence | topic Makefile + board.mk | `board_smoke` 或分步 board test/bench/fetch | 板卡结果进入 result，Evidence Doctor 暴露 warning / suggestion。 |

## Evidence Doctor 和 registry 规则

本阶段先用 `test-rvv/script/evidence_doctor.py --summary-md` 对 `log/board/analyze_bench_compare.log` 做轻量 Evidence Doctor 检查。若本阶段生成 topic-local manifest（证据清单），再改用 JSON manifest。当前没有 registry 时，result 写 `evidence_registry_status=not_available` 并列出人工 freshness check（新鲜度检查）路径。

## 板卡复跑预算和决策桶

默认先运行 1 次 board compare。若任一候选处在 `0.95x-1.05x` 或方向与 correctness/asm 预期冲突，最多扩大到 5-run repeated 手工预算。decision bucket（决策桶）：`positive >= 1.20x`、`weak-positive 1.05x-1.20x`、`neutral 0.95x-1.05x`、`negative < 0.95x`、多批方向不稳为 `unstable`。

## 继续 / 停止条件

只要 correctness、asm、board 或 Evidence Doctor 仍未闭合且板卡可用，本阶段继续推进。合法停止条件是：继续需要修改 production 源码而尚未进入 PI1、板卡/工具不可达、Evidence Doctor Error 不能修复、或 dirty isolation 显示本轮会覆盖无关 topic。

## 文档更新清单

本阶段结束前更新 `result.zh.md`、`optimization-matrix.zh.md`、`optimization-roadmap.zh.md`、`sac_model_sphere-evaluation.zh.md` 和最终 Handoff。`doc-rvv/sample_consensus/sac_model_sphere-RVV.zh.md` 当前 `not_applicable`，因为还没有用户确认采纳的 production 行为或 PI5 证据闭环。
