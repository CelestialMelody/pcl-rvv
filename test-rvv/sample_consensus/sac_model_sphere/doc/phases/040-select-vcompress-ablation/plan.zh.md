# Phase 040: selectWithinDistance vcompress 消融计划

## 阶段意图和边界

本阶段尝试一个新的 `selectWithinDistance` 实现族：在 RVV chunk 内用 `vcompress`（按 mask 压缩有效 lane）
只保留命中的 index 和平方距离，再用标量 `sqrt` 写 `error_sqr_dists_`。目标是回答它是否比 Phase 020
已采纳的 production helper 更快。

本阶段先只修改 `test-rvv/sample_consensus/sac_model_sphere/` 下的测试、bench、manifest 和阶段文档，
不替换 production `selectWithinDistanceRVV`。若板卡结果显示收益，才进入后续 production integration loop。

## 当前状态

| 对象 | 状态 |
| --- | --- |
| production baseline | Phase 020 已采纳 `selectWithinDistanceRVV`，5-run board median `1.5020x`。 |
| 正确性 | `run_test_compare` 已有 4 个 gtest，覆盖 public / Standard / RVV helper 对拍。 |
| 反汇编 | `selectWithinDistanceRVV` 符号级 RVV 指令数为 `17`。 |
| Evidence Doctor | Phase 020 production select 行无 Error / Warning；两个 Error 属于未接入的 `getDistancesToModel`。 |

## 候选与 A/B 边界

| question | answer |
| --- | --- |
| evidence role | production-detail diagnostic（生产细节诊断，不替代 production direct） |
| A/B boundary | 同一 bench binary 内的 `public selectWithinDistance` 当前 production helper vs test-only `vcompress` helper |
| 当前决策问题 | RVV-family-selection（RVV 实现族选择） |
| diagnostic 是否可外推到 production | 只能说明是否值得进入后续 production probe；不能直接采纳。 |
| comparison-boundary / baseline mismatch 风险 | 有。baseline 是 public overload，candidate 是 test-only helper；结论需降级为 implementation-family comparison input。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 不允许。只有候选在同边界 RVV-vs-RVV 明显正向且 correctness/asm/doctor 闭合时才进入生产探针。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 需要；本阶段若正向，下一 phase 必须把候选接入 production 后重跑 PI2-PI5。 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | correctness | bench / board | asm | doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- |
| current production select RVV | direct indexed `indices_` | `PointXYZ`, float xyz layout | 已通过 | Phase 020 median `1.5020x` | `rvv_instr_count=17` | select row clean | baseline |
| select `vcompress` candidate | direct indexed `indices_` | `PointXYZ`, float xyz layout；correctness 可复用 `PointXYZI` 但 board 先只跑 `PointXYZ` | 待新增 gtest | 待新增 bench item 和 board repeated | 待 dump | 待 production-detail manifest | planned |

## 实现和测试动作

| 动作 | 产物 / 命令 | 完成判据 |
| --- | --- | --- |
| 新增 test-only helper | `src/test_sac_model_sphere.cpp`、`src/bench_sac_model_sphere.cpp` | helper 使用 `vcompress` 压缩 index 和平方距离，保持输出顺序与精确 `sqrt` 误差写回。 |
| 新增 correctness | `make -C test-rvv/sample_consensus/sac_model_sphere run_test_compare` | Std/RVV 构建通过；RVV 构建下 vcompress candidate 与 Standard helper 对拍。 |
| 新增 bench label | `src/bench_sac_model_sphere.cpp` 输出 `vcompress candidate selectWithinDistance` | Std/RVV 输出 checksum 可比较；计时边界只包含 helper 调用。 |
| 反汇编 | `make -C test-rvv/sample_consensus/sac_model_sphere clean_bench_rvv dump_bench_rvv` | RVV binary 中可见 `vcompress` 指令或候选内联区域有压缩指令。 |
| 板卡 repeated | 若本地构建通过，运行 `collect_production_repeated_board_evidence` 后扩展 manifest / doctor | 5-run 结果能比较 current production helper 与 vcompress candidate。 |

## 板卡预算和决策桶

使用 5-run repeated board，`BENCH_ARGS=65536 200`，warmup 为 5。若 vcompress candidate 相对当前 production
helper 的 RVV-vs-RVV B/A 明显大于 1 且 doctor 无阻塞 Error，标为 production probe candidate；若小于或接近 1，
标为 rejected for current input；若跨 1 且波动大，标为 unstable，不继续自动复跑。

## 文档更新清单

阶段结束后更新 `result.zh.md`、optimization matrix、optimization roadmap、`benchmark-and-evidence.zh.md`、
`optimization-evidence.zh.md` 和 evaluation。若进入生产探针，后续再更新 `doc-rvv`。
