# Phase 080 计划：interpolation bin-selection scalar-tail diagnostic

## 阶段意图和边界

本阶段只做 test-only diagnostic（测试专用诊断）：在 Phase 050 已经生成 `x/y/z` 投影、`distance` 和 `bin_distance` 之后，抽出 descriptor volume（描述子三维体）bin selection（桶选择）相关的窄片段，诊断 `desc_index`、`step_index`、相邻桶 residual（残差）和中心桶权重是否能由 RVV 批量算术辅助。

本阶段不证明完整 `interpolateSingleChannel` / `interpolateDoubleChannel`，不覆盖 `std::acos` / `std::atan2`、半径 / 倾角 / 方位角三组插值、histogram scatter（直方图离散写入）、完整 descriptor 写回，也不修改 `features/include/pcl/features/impl/shot.hpp`。

## 当前状态清单

| item | 当前状态 |
| --- | --- |
| production 源码 | 未修改；PI2 生产接入需要用户明确授权。 |
| Phase 050 | interpolation geometry staging arrays 修正 valid mask 后仍为 0.97x，不建议作为生产探针输入。 |
| Phase 070 | indexed RGB/LUT scalar staging + RVV LAB distance 为 1.02x，不建议该 staging 形态进入 production probe。 |
| 可继续动作 | 在未授权 production 的边界内，继续更窄的 test-only interpolation 子片段诊断。 |

## 假设与候选族

候选假设：分支选择、invalid lane（无效向量通道）判断和 bucket id 仍使用标量逻辑，RVV 只处理 `residual = |bin_distance - center|` 与 `center_weight = 1 - residual` 这类无分支算术。若该窄片段在板卡上稳定正向，后续才考虑是否把它作为 production-shaped diagnostic（生产形态诊断）输入；若不稳定或收益不足，则不建议继续该 code shape（代码组织形态）。

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness target | bench target | board evidence | asm boundary | doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| interpolation bin-selection scalar-tail staging | indexed surface batch after geometry staging | double staging arrays, descriptor bin ids as scalar output | `computeInterpolationBinSelectionScalar/RVV` test-only helper | `run_test_compare` 新增 gtest | `interpolation_bin_selection_component` | 3-run targeted board budget | `dump_bench_rvv` 归属 helper 算术 | `run_evidence_doctor` | 待定 |

## 实现和测试动作

| action | 产物 / 命令 | 完成判据 |
| --- | --- | --- |
| TDD red | 在 `src/test_shot.cpp` 新增 `ShotInterpolationBinSelectionComponent` 测试后运行 `make -C test-rvv/features/shot run_test_compare` | 缺少 helper 导致编译失败，证明测试先暴露缺口。 |
| helper implementation | 在 `include/impl/shot_interpolate.hpp` 增加标量参考和 RVV 候选 | Std / RVV 输出 `desc_index`、`step_index`、相邻桶 index / delta、中心桶权重一致；invalid lane 写回清零。 |
| bench | 在 `src/bench_shot.cpp` 增加 `interpolation_bin_selection_component` case | 可由 `BENCH_ARGS="--case-filter interpolation_bin_selection_component"` 单独运行。 |
| manifest | 更新 `script/generate_shot_evidence_manifest.py` | Evidence Doctor 能识别该 case label（用例标签）。 |
| asm | `make -C test-rvv/features/shot dump_bench_rvv` | 能看到 RVV load/sub/abs/reverse-sub/store 算术序列。 |
| board | `make -C test-rvv/features/shot run_board_bench_compare BENCH_ARGS="--case-filter interpolation_bin_selection_component"`，预算 3 次 targeted run | 若 decision bucket（决策桶）摇摆，标为 `unstable`，不继续无限复跑。 |
| doctor | `make -C test-rvv/features/shot fetch_board_logs && make -C test-rvv/features/shot run_evidence_doctor` | Error 必须为 0；Warning 需要解释。 |

## Diagnostic 到 production mismatch audit

| question | answer |
| --- | --- |
| evidence role | diagnostic（诊断证据） |
| A/B boundary | test helper（测试 helper） |
| 当前决策问题 | implementation-shape：这个 scalar-tail staging 形态是否值得继续生产形态探针。 |
| diagnostic 是否可外推到 production | unknown。它只覆盖 geometry projection 之后的桶选择算术，不覆盖完整 production interpolation。 |
| comparison-boundary / baseline mismatch 风险 | yes。production 中该逻辑和 `acos` / `atan2`、histogram scatter、descriptor 写回混在同一函数中。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 不推荐以本候选单独进入 production probe；只有后续 production profile 证明 bin-selection 是独立主瓶颈，且 PI1 重新冻结范围时才允许。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | yes。本阶段不能 clean-adopt。 |

## 板卡复跑预算和决策桶

预算为 3 次 targeted board run。判断口径：

- `positive`：3 次均明显高于 1.10x 且 checksum 一致。
- `weak-positive`：3 次均高于 1.00x，但接近阈值。
- `neutral / negative`：多数 run 不高于 1.00x。
- `unstable`：预算内出现 negative / positive decision bucket 摇摆。

预算用完且 bucket 摇摆时停止该证据动作并降级为 `attempted / unstable`。

## 文档更新清单

完成后更新本 result、phase index、optimization matrix、roadmap、evaluation、topic README 和 `doc-rvv/library-screening/features/features-function-evaluation-queue.zh.md` 队列表。生产源码和长期 `doc-rvv` 主题文档保持不适用。

## 继续 / 停止条件

若本阶段为稳定正向，只能进入更接近 production boundary 的显式 PI1 计划；若 neutral、negative 或 unstable，则不建议继续该候选，默认下一阶段转向 `090-structure-parity-doc-suite-diagnostic`，补齐 topic-local doc suite（主题本地文档套件）和结构对齐审计。板卡当前可用；“需要板卡验证”不是停止条件。
