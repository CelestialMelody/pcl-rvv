# Phase 020 Result: selectWithinDistance 点类型扩展

## 执行范围

本阶段按 `plan.zh.md` 验证 `SampleConsensusModelCircle3D<PointT>::selectWithinDistance` RVV（RISC-V Vector，可变长向量扩展）生产路径在 `PointXYZI`、`PointXYZRGB` 和 `PointXYZRGBA` 上是否仍有接入价值。实际执行后发现三个点型的 production-public（公开入口标量 / RVV）板卡结果均为负向，因此生产 gate 已被收窄到 exact `pcl::PointXYZ`；这些点型在 RVV 构建中必须走 scalar fallback（标量回退）。

`countWithinDistance` 和 `getDistancesToModel` 不在本阶段修改。当前阶段只关闭 direct indexed `indices_`、`Scalar=float` model coefficients、当前三个点型、65536 点、200 次计时迭代、20 次 warm-up、Milkv-Jupiter 板卡这一范围。

## 动作回填

| action | 状态 | 命令 / 证据 | 结论 |
| --- | --- | --- | --- |
| 点类型输入和 bench CLI | done | `src/test_sac_model_circle3d.cpp`、`src/bench_sac_model_circle3d.cpp` | 测试和 bench 可用第四参数选择 `PointXYZI`、`PointXYZRGB`、`PointXYZRGBA`。 |
| 三点型 repeated board | done | `make -C test-rvv/sample_consensus/sac_model_circle3d record_select_xyzi_board_evidence_state record_select_xyzrgb_board_evidence_state record_select_xyzrgba_board_evidence_state` | 三个点型均出现高频退化，不支持扩大 production RVV gate。 |
| fallback 收窄 | historical evidence | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_circle3d.hpp` | 阶段内曾把 RVV helper 收窄到 exact `pcl::PointXYZ`；最终 closeout 已回滚整个 production helper。 |
| fallback correctness | done | `make -C test-rvv/sample_consensus/sac_model_circle3d run_test_compare` | 当前源码没有生产 RVV helper，Std/RVV 构建的公开入口均保持标量语义。 |
| production asm attribution | historical evidence | `make -C test-rvv/sample_consensus/sac_model_circle3d clean_bench_rvv check_select_production_asm` | 该 helper 已随回滚移除；此处仅保留历史 asm 归属证据。 |

## 板卡性能摘要

B/A = Std public ms / RVV public ms，大于 1 表示 RVV 更快。

| point type | B/A values | mean | median | positive count | doctor |
| --- | --- | ---: | ---: | ---: | --- |
| `PointXYZI` | 1.0633, 0.8650, 0.8707, 0.8797, 0.8800 | 0.9117 | 0.8797 | 1/5 | Errors=1, Warnings=1, Suggestions=0 |
| `PointXYZRGB` | 0.8882, 0.8879, 0.8922, 0.8860, 0.8870 | 0.8883 | 0.8879 | 0/5 | Errors=1, Warnings=0, Suggestions=0 |
| `PointXYZRGBA` | 0.8890, 0.8746, 0.9056, 0.8532, 0.8915 | 0.8828 | 0.8890 | 0/5 | Errors=1, Warnings=0, Suggestions=0 |

## Evidence Doctor 结果

三个点型的 Evidence Doctor（证据体检）都报告 `ba_degradation_frequency` Error。处理动作是拒绝将这些点型纳入 production RVV 范围，并增加 fallback correctness 测试，确保它们在 RVV build 中仍走标量路径。`PointXYZI` 额外有 long-tail / variance warning；由于 mean、median 和正向次数都不支持采纳，本阶段不追加复跑。

## Diagnostic 到 Production Mismatch Audit

| question | answer |
| --- | --- |
| evidence role | production-public。 |
| A/B boundary | Std binary public overload vs RVV binary public overload；两侧 wrapper、row source、timer boundary 和 checksum policy 一致。 |
| 当前决策问题 | traits-gated `selectWithinDistance` 生产 RVV 是否能扩展到更多常见 xyz AoS 点型。 |
| diagnostic 是否可外推到 production | 不依赖 diagnostic 外推；本阶段使用真实 public entry repeated board。 |
| comparison-boundary / baseline mismatch 风险 | 低；允许 `gate` / `reduction` 差异作为目标变量。 |
| 弱 / 负 / 中性 / 不稳定时是否允许继续 | 当前三点型均负向；不扩大为泛型采纳。 |
| clean adoption 是否需要 RVV-vs-RVV detail A/B | 当前不是 RVV-family-selection；结果已经足以拒绝此 gate 扩展。 |

## EvidenceDecision

本阶段 decision 是 `point-type expansion rejected with evidence`：

- `PointXYZI`、`PointXYZRGB`、`PointXYZRGBA` 不建议接入 RVV production path。
- 当前源码已回滚生产 RVV helper，因此这些点型和 `PointXYZ` 均走既有标量公开入口。
- 该阶段反过来要求重新检查 Phase 010 `PointXYZ` 接入后的当前证据；后续 10-run post-narrowing board evidence 显示 `PointXYZ` 也不满足采纳门槛。

## Continue / Stop Decision

`continue_stop_decision = phase_closed_with_rejected_expansion`

本阶段没有未阻塞的点类型扩展动作。由于 Phase 010 post-narrowing 生产证据已经出现 Evidence Doctor Error，当前 topic 不应继续扩大 production 范围；生产补丁已回滚，当前 closeout 为 `rollback/no-production`。
