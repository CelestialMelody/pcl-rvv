# Phase 048 计划：row-source shuffle staged-selected-cloud detail A/B

## 阶段意图和边界

本阶段继续优化矩阵里的 `staged-selected-cloud-detail-ab`（暂存选中点云细节对照）：只在 test-rvv bench 里比较 current row-source RVV path（当前行来源 RVV 路径）与 staged selected-cloud path（先把 shuffled index / correspondence 对应的 source / target 点复制成顺序点云，再走 ordered public scale 路径）。

本阶段不修改 production 源码，不改变 Phase 047 已接入的 correspondence sorted-copy production probe，也不把 staged selected-cloud 写成 adopted behavior。

验证范围：

- row source：dual-indexed 和 correspondence。
- 点型 / `Scalar` / layout：`PointXYZ -> PointXYZ`、`float`、dense xyz AoS。
- order pattern：deterministic shuffle。
- 规模：4K / 64K / 256K。
- 计时边界：candidate 必须包含 selected source / target cloud 的拷贝成本和 ordered public estimate 成本。

不证明的范围：

- source-indexed、ordered-cloud-pair 或规则 contiguous / stride / reverse 顺序。
- `Scalar=double`、非 dense、非法 index / correspondence、全部泛型点型。
- production dispatch 或 fallback 行为。

## 当前状态清单

| 项目 | 当前事实 |
| --- | --- |
| Phase 045 | shuffle 在 dual-indexed / correspondence 64K / 256K 明显弱于 contiguous / stride / reverse，但仍 positive。 |
| Phase 046 | sorted-copy 对 correspondence 64K / 256K positive；dual-indexed mixed；4K rejected。 |
| Phase 047 | correspondence sorted-copy production probe 已完成 correctness、QEMU smoke、board repeated、Doctor 和 registry；用户已允许有收益实现接入。 |
| test support | 已有 `selectPointCloudByIndices`，可构造 selected source / target cloud。 |

## 假设与候选族

| candidate family | 假设 | 风险 |
| --- | --- | --- |
| `staged-selected-cloud-detail-ab` | 对 shuffled dual-indexed / correspondence，先把两端点复制成顺序点云后，ordered public RVV path 的 stride load（跨步加载）可能抵消双 gather（离散加载）的局部性损失。 | 两端点云拷贝比 sorted correspondence 更重；4K 很可能被拷贝成本吞掉；如果只比 public Std/RVV，会混淆为生产收益，因此必须做同一 RVV binary 内 detail A/B。 |

## 优化矩阵

| candidate family | row source | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `staged-selected-cloud-detail-ab` | dual-indexed | `PointXYZ -> PointXYZ` / `float` / dense xyz AoS | shuffled dual indices 4K / 64K / 256K | 复用 Phase 046 selected-cloud reference；bench 输出 `max_reference_error` | 新增 `row-source-shuffle-staged-selected-cloud-detail-ab` case-filter | board repeated detail A/B | same RVV binary；candidate 走 ordered public RVV helper，拷贝为 scalar pre-pass | Doctor 解释 copy + estimate 边界、4K 退化和 long-tail | pending |
| `staged-selected-cloud-detail-ab` | correspondence | `PointXYZ -> PointXYZ` / `float` / dense xyz AoS | shuffled correspondences 4K / 64K / 256K | 同上 | 同上 | board repeated detail A/B | 同上 | 同上 | pending |

## 实现和测试动作

| 动作 | 产物 | 完成判据 |
| --- | --- | --- |
| A1 bench case | `src/bench_tesvd_scale.cpp` | 新增 current vs staged-selected-cloud paired labels；candidate 计时包含 selected cloud 构造。 |
| A2 summary / manifest | `script/generate_tesvd_scale_detail_ab_summary.py` | 支持 staged-selected-cloud label 和 evidence role，生成 summary / manifest。 |
| A3 Make target | `Makefile` | 新增 QEMU smoke、board repeated、Evidence Doctor 和 registry target。 |
| A4 evidence | QEMU / board summary | QEMU 只看日志形状；board 5-run 决定 positive / weak / negative / unstable。 |
| A5 docs | result、matrix、roadmap、testing / bench docs | 记录决策和下一阶段恢复入口。 |

## Evidence Doctor 和 registry 规则

- QEMU timing 不作为性能结论，只验证 label、`max_reference_error`、manifest shape 和 Doctor 输入。
- board repeated 才支撑 RVV-vs-RVV family comparison。
- strict A/B 的 baseline 是 current shuffled row-source RVV path；candidate 是 staged selected-cloud + ordered public RVV path。`timer_boundary` 有意不同，manifest 必须在 `allowed_contract_mismatches` 写明。
- 生成或覆盖 summary / manifest / doctor 后必须登记到 `log/evidence_registry.json`，并运行 `make evidence_status`。

## 阶段完成条件

| 状态 | 条件 |
| --- | --- |
| `positive_production_probe_candidate` | 某个 row source / size 组合 repeated board B/A 明确 positive，且 correctness / Doctor 可解释；只能进入后续有界 production probe，不能本阶段采纳。 |
| `attempted_negative_or_mixed` | candidate 被 copy/staging 成本吞掉、4K 退化或 64K / 256K 不稳定；保留为诊断记录，不接 production。 |
| `blocked` | board 不可用、脚本无法生成 manifest、或 staged candidate 的 correctness 与 selected-cloud reference 不一致。 |

## 板卡复跑预算和决策桶

默认 5 runs、warmup 5、iteration 20。`B/A > 1.2` 且 min 不退化为 positive；median `1.05-1.2` 且 min >= `0.97` 为 weak-positive；median < `0.97` 或任一关键边界 5/5 `< 1` 为 negative；方向摇摆则 unstable。

## 继续 / 停止条件

若 staged selected-cloud 在 dual-indexed 或 correspondence 64K / 256K 上显著正向，下一阶段只能是 bounded production probe plan，并且必须重新审计 copy 成本、内存分配、fallback、真实公开入口和用户确认。若结果 mixed / negative，则将该路线写成 attempted，默认回到已接入的 correspondence sorted-copy 和现有 row-source path。

## 文档更新清单

完成后同步本 result、phase README、optimization matrix、optimization roadmap、README、testing overview、benchmark-and-evidence、optimization-evidence 和 test-support code map。production 长期 `doc-rvv` 只记录已采纳行为；本阶段若不接 production，只能写成 topic-local 诊断。

## diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | `production_detail_rvv_vs_rvv`，但仍是 test-rvv detail A/B，不是 production public adoption evidence。 |
| A/B boundary | current shuffled row-source public RVV path vs staged selected-cloud + ordered public RVV path。 |
| 当前决策问题 | 是否存在比 current gather / sorted-copy 更值得继续的 shuffled row-source implementation family。 |
| diagnostic 是否可外推到 production | 只能部分外推。拷贝成本计入了 bench，但真实 production probe 还需要 dispatch、fallback、异常语义、内存分配和用户确认。 |
| comparison-boundary / baseline mismatch 风险 | 有，candidate 的 row-source 外壳与 baseline 不同；因此只能作为 family-selection 诊断。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 不允许；只有明确 positive 子边界才能开 production probe。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 需要。本阶段若 positive，也只是下一阶段 production probe candidate。 |
