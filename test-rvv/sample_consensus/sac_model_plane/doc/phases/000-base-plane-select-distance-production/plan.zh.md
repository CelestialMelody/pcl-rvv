# Phase 000: base-plane select/getDistances production 补齐计划

## 阶段意图和边界

本阶段把 `sac_model_plane.hpp` 已有的 `countWithinDistanceRVV` 扩展成三入口一致的生产路径：`selectWithinDistance` 使用 mask + `vcompress`（按掩码压缩写回）输出 inliers 与距离，`getDistancesToModel` 使用同一平面距离 RVV kernel（内核）稠密写回 `std::vector<double>`。阶段范围只覆盖 direct indexed `indices_` 入口、registered single-float x/y/z 点型、`Eigen::VectorXf` float 系数和 `double threshold` 转 float 比较；不覆盖 normal-plane、sphere/circle、SAC 方法后处理，也不声称所有点型/布局都已完成。

## 当前状态清单

| area | current state | path |
| --- | --- | --- |
| production count | `countWithinDistance` 已在 `__RVV10__` 且 `RVVXYZFloatLayout<PointT>` 成立时分流到 `countWithinDistanceRVV`。 | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_plane.hpp` |
| production select/getDistances | 两个入口仍直接执行标量循环，没有 Std/RVV helper 分层。 | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_plane.hpp` |
| declarations | header 只声明 count 的 Standard/SSE/AVX/RVV helper。 | `sample_consensus/include/pcl/sample_consensus/sac_model_plane.h` |
| tests | `plane_models` 历史测试覆盖 count 和 SAC 回归；本 topic 尚无独立 production direct 测试。 | `test-rvv/sample_consensus/plane_models/src/test_sample_consensus_plane_models.cpp` |
| board | 用户说明板卡可用；本阶段需要运行 board smoke/bench，不能停在“需要板卡”。 | prompt + `test-rvv/mk/rvv-topic.mk` |

## validated_scope / unvalidated_scope

| scope type | 内容 |
| --- | --- |
| validated_scope | `SampleConsensusModelPlane<PointXYZ>`，direct indexed `indices_`，float xyz layout，`selectWithinDistance` / `countWithinDistance` / `getDistancesToModel` 公开入口。 |
| unvalidated_scope | 非 `PointXYZ` 但 registered float xyz 的点型、非 float xyz 字段、超大 cloud 触发 32-bit byte offset gate、未来 source/correspondence 类入口。 |
| point_type_expansion_queue | Phase 010：补 `PointXYZI` 或等价 registered float xyz 点型 public-entry correctness、fallback 和必要 board/asm；若本阶段生产 gate 已泛化，010 只验证泛型结论。 |
| phase_closeout_boundary | 本阶段只能关闭 base-plane 三入口在当前点型/布局/row source 下的 production direct 证据，不能关闭 sample_consensus 其它模型。 |

## diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | production-public 和 production-detail；测试直接包含生产 header，bench 对比 `USE_PCL_RVV10=0/1` 两个构建。 |
| A/B boundary | public overload + protected helper；`select` / `count` / `getDistances` 的计时边界只包含模型对象上的目标入口调用，不包含输入构造。 |
| 当前决策问题 | RVV-vs-scalar 和 fallback correctness。 |
| diagnostic 是否可外推到 production | 不依赖外推；本阶段补真实 production dispatch（生产分流）证据。 |
| comparison-boundary / baseline mismatch 风险 | 有；Std/RVV 是两个构建，必须保持相同输入、indices、系数、threshold、warmup 和 iteration。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 若 correctness/asm 通过但 board 弱或中性，只能保留 bounded production candidate，不能 clean adopt。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 本阶段不是新 RVV family 替换旧 family；只把已有 count family 扩到两个未接入口，Std/RVV public positive 可支持 bounded production candidate，最终仍需 PI5 用户确认。 |

## 实现和测试动作

| action | files | command / evidence | completion |
| --- | --- | --- | --- |
| 写 Std/RVV helper 分层 | `sac_model_plane.h/.hpp` | 编译 `run_test_compare` | public entry 仅做 validation、dispatch 和 fallback |
| 新增独立 test harness | `test-rvv/sample_consensus/sac_model_plane/src/test_sac_model_plane.cpp` | `make -C test-rvv/sample_consensus/sac_model_plane run_test_compare` | Std/RVV 两侧通过，direct helper 与 public entry 输出一致 |
| 新增 bench harness | `src/bench_sac_model_plane.cpp` | board `run_board_bench_compare` | 输出 `Dataset:`、`Iterations:`、`Warmup Iterations:` 和三行 `ms/iter` |
| QEMU correctness | topic Makefile | `run_test_compare` | QEMU 只作为 correctness / log-shape，不作为性能结论 |
| asm attribution | topic Makefile | `dump_bench_rvv` | helper 符号内出现 RVV 指令 |
| board evidence | topic Makefile + board.mk | `board_smoke` 或分步 board test/bench/fetch | 板卡结果进入 phase result，Evidence Doctor 暴露 warning |

## Evidence Doctor 和 registry 规则

本阶段先使用 `test-rvv/script/evidence_doctor.py --summary-md` 对 `log/board/analyze_bench_compare.log` 做轻量检查；若本阶段生成 topic-local manifest，则改用 JSON manifest。当前无 registry 时，result 写 `evidence_registry_status=not_available` 并列出人工 freshness 检查路径。

## 板卡复跑预算和决策桶

默认运行 1 次 board compare 作为本阶段生产性能入口；若结果在 `0.95x-1.05x` 或与预期相反，最多增加到 5-run repeated 手工预算。decision bucket：`positive >= 1.20x`、`weak-positive 1.05x-1.20x`、`neutral 0.95x-1.05x`、`negative < 0.95x`、多次方向不稳为 `unstable`。

## 继续 / 停止条件

只要 correctness、asm、board 或 Evidence Doctor 仍未闭合且板卡可用，本阶段继续推进。合法停止条件只包括：生产接入 PI5 需要用户确认、板卡/工具不可达、Evidence Doctor Error 不能修复、或 dirty isolation 显示当前阶段会覆盖无关 topic。

## 文档更新清单

本阶段结束前更新：`result.zh.md`、`optimization-matrix.zh.md`、`optimization-roadmap.zh.md`、`sac_model_plane-evaluation.zh.md` 和 Handoff 摘要。`doc-rvv/sample_consensus/sac_model_plane-RVV.zh.md` 只有 PI5 用户确认采纳后才适用，本阶段先不创建。
