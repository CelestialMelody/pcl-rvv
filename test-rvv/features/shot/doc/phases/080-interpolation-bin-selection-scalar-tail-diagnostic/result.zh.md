# Phase 080 结果：interpolation bin-selection scalar-tail diagnostic

## 执行范围

本阶段实际执行范围与计划一致：只修改 `test-rvv/features/shot` 下的测试专用 helper、gtest、bench case 和 Evidence Doctor manifest wrapper。`features/include/pcl/features/impl/shot.hpp` 未修改，生产接入 PI2 仍需要用户明确授权。

本阶段 helper 把 descriptor volume bin selection（描述子三维体桶选择）拆成两层：bucket id、`desc_index`、`step_index` 和有效性仍由标量 lane（向量通道）逻辑判断；RVV 只批量计算相邻桶 residual（残差）和中心桶权重。它不覆盖完整 interpolation、`acos` / `atan2`、histogram scatter 或 production dispatch。

## 计划动作回填

| action | 状态 | 命令 / 证据 | 结论 |
| --- | --- | --- | --- |
| TDD red | done | `make -C test-rvv/features/shot run_test_compare` 在新增测试后因 `computeInterpolationBinSelectionScalar/RVV` 缺失失败。 | 测试先暴露 helper 缺口。 |
| helper implementation | done | `include/impl/shot_interpolate.hpp` 新增 `InterpolationBinSelectionLane`、标量参考和 RVV 候选；invalid lane 写回清零。 | Std / RVV 输出一致；初版 invalid lane 中心权重为 1 的问题已修正。 |
| correctness | done | `make -C test-rvv/features/shot run_test_compare` | Std / RVV 两侧各 12 个 gtest 通过。 |
| bench case | done | `src/bench_shot.cpp` 新增 `interpolation_bin_selection_component`；`BENCH_ARGS="--case-filter interpolation_bin_selection_component"` 可定向运行。 | bench label 已进入 topic case 字典。 |
| manifest | done | `script/generate_shot_evidence_manifest.py` 新增 `interpolation_bin_selection_component` metadata。 | 当前 fetched report 能由 Evidence Doctor 解析。 |
| asm | done | `make -C test-rvv/features/shot dump_bench_rvv`；`bench_shot_rvv.asm` 中可见 `vle64.v`、`vfsub.vv`、`vfabs.v`、`vfrsub.vf`、`vse64.v`。 | RVV 算术存在，但只归属到 test-only residual / center-weight helper。 |
| board targeted runs | done | `run_board_bench_compare BENCH_ARGS="--case-filter interpolation_bin_selection_component"` 三次 targeted run。 | 0.84x、1.12x、1.17x，checksum 一致；bucket 从 negative 摇到 positive。 |
| Evidence Doctor | done / limited | `fetch_board_logs && run_evidence_doctor`；`log/board/evidence_doctor.md` 当前为 Errors=0、Warnings=1、Suggestions=0。 | Doctor 只看当前 fetched 最后一轮 1.17x，Warning 为 low-run；跨 run 不稳定需要人工记录。 |

## Board evidence（板卡证据）

历史 all-case smoke（误用 `CASE_FILTER` 而不是 `BENCH_ARGS`）中，该 case 为 Std 4.4023 ms、RVV 6.5703 ms、约 0.67x，checksum match（校验和一致）。该结果只作为历史 smoke，不作为当前 targeted summary。

三次 targeted board run：

| run | Std avg | RVV avg | speedup | checksum |
| --- | --- | --- | --- | --- |
| targeted 1 | 4.53815 ms | 5.40120 ms | 0.84x | match |
| targeted 2 | 5.13229 ms | 4.59604 ms | 1.12x | match |
| targeted 3 / current fetched | 5.29294 ms | 4.52271 ms | 1.17x | match |

rerun budget（复跑预算）已用完，decision bucket（决策桶）在 negative 与 positive 之间摇摆，因此本阶段结论是 `attempted / unstable`。不要继续自动复跑来追单个好看的数值；若以后恢复，必须先改变 evidence question（证据问题），例如补 production profile 或更细的 scalar-tail 消融。

## Diagnostic 到 production mismatch audit 回填

| question | answer |
| --- | --- |
| evidence role | diagnostic（诊断证据） |
| A/B boundary | test helper（测试 helper） |
| 当前决策问题 | implementation-shape：更窄的 bin-selection scalar-tail staging 是否值得继续。 |
| diagnostic 是否可外推到 production | no for decision。它只覆盖 geometry projection 之后的桶选择局部算术，不能代表完整 interpolation。 |
| comparison-boundary / baseline mismatch 风险 | yes。production 中还有 `acos` / `atan2`、半径 / 倾角 / 方位角插值、histogram scatter 和 descriptor 写回。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 本候选不推荐单独进入 production probe；不稳定只说明该 diagnostic boundary 当前不支持继续。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | yes。本阶段没有 clean adoption 条件。 |

## Evidence Doctor（证据体检）

当前 `log/board/evidence_doctor.md`：

- Errors=0。
- Warnings=1：`low_run_count`，因为当前 fetched summary 只解析到 1 个 repeated value。
- Suggestions=0。

处理动作：Doctor report 可证明最后一轮日志没有 checksum / manifest error，但不能覆盖三次 targeted run 的跨批次摇摆。phase result 以三次 targeted run 的人工记录为准，将候选降级为 `attempted / unstable`。

## Optimization matrix 更新

`interpolation bin-selection scalar-tail staging` 从 roadmap deferred 升级为 attempted / unstable：

- correctness：Std / RVV 12 个 gtest 通过。
- bench：case 已接入，可定向运行。
- board：0.84x、1.12x、1.17x，checksum match，但 bucket 不稳定。
- asm：RVV residual / center-weight 算术可见，branch selection 仍是标量 staging。
- production decision：生产源码未修改，不进入 PI2，不作为 production probe 默认输入。

## 阶段反思

Phase 050 说明多组 geometry staging arrays 成本太高；Phase 080 说明即使缩窄到 bin-selection residual / center-weight 算术，scalar-tail staging 仍不稳定。当前未授权 production 的剩余工作面中，继续堆插值局部 helper 的边际价值下降。下一阶段应先补结构和文档套件，让 reviewer 能在没有聊天上下文时复核测试入口、bench case、Evidence Doctor 边界和候选取舍。

## Continue / stop decision

- `continue_stop_decision`：继续。
- `stop_condition_hit`：none。生产源码仍未授权，但 topic-local doc suite / structure parity 在当前授权范围内且无 blocker。
- `next_phase_default`：`090-structure-parity-doc-suite-diagnostic`。
