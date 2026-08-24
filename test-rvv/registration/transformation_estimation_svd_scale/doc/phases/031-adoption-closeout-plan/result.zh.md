# Phase 031 结果：adoption closeout

## EvidenceDecision

`adopted-by-user / direct-fused-scale-accum / ordered-cloud-pair / traits-gated xyz AoS / Scalar=float`。

用户已确认当前有收益的 production patch 可以接入。本阶段把 Phase 010/030 的 PI5 pending 状态收口为 adopted production behavior（已采纳生产行为），并同步 topic-local doc suite 与 `artifact_layout.topic_doc_template` 解析出的 production 长期主题文档：

- `doc-rvv/registration/transformation_estimation_svd_scale-RVV.zh.md`

## 采纳范围

| 维度 | 已采纳范围 |
| --- | --- |
| production entry | ordered-cloud-pair public overload。 |
| point type / layout | traits-gated xyz AoS；当前采纳证据的直接板卡数据仍以 `PointXYZ -> PointXYZ` 为代表。 |
| Scalar | `float` only。 |
| input shape | source / target 等长、dense、`nr_points >= 16`、source variance 非退化。 |
| fallback | 非 RVV、非 float、layout 不满足、小规模、非 dense、退化 source、indexed / correspondence 等路径仍回父类 scale 标量路径。 |

## 证据链

| evidence | 当前结论 |
| --- | --- |
| QEMU correctness | 当前 `run_test_compare` 已扩展为 8 tests，Std/RVV 均通过；新增测试只补泛型点型 correctness，不改变已采纳板卡结论。 |
| QEMU public-scale smoke | Phase 010 public-scale smoke Doctor `Errors=0`、`Warnings=0`；QEMU timing 不作为性能证据。 |
| ASM attribution | production ordered overload 符号内可见 `vlsseg3e32.v`、`vfmacc`、`vfredosum`、`vsetvli`。 |
| board production direct | `log/board/production_public_scale_ordered_cloud_pair_repeated/summary.md`：4K/64K/256K median B/A = `26.123x` / `33.860x` / `33.066x`。 |
| Evidence Doctor | production board Doctor `Errors=0`、`Warnings=1`、`Suggestions=0`；4K warning 按 size 分开解释，不阻塞当前窄范围采纳。 |
| registry | adoption 后需重新记录 QEMU correctness 8-test 状态并保持 `evidence_status` fresh。 |

## 同步结果

| area | 结果 |
| --- | --- |
| production source | 当前 patch 保留，不回滚；未扩大到 indexed / correspondence。 |
| production_topic_doc | 已创建长期生产文档，只写 adopted production behavior、dispatch / fallback、证据链和未覆盖范围。 |
| topic README / evaluation / matrix / roadmap | 从 pending user confirmation 更新为 adopted；默认恢复入口切到后续优化 phase。 |
| rollback plan | Phase 032 仅作为历史备选计划保留，不再是默认路径。 |
| next optimization | `generic-point-type-expansion` 解除 PI5 blocker，进入 Phase 040 correctness scout。 |

## 保留风险

- 泛型点型直接板卡 evidence 尚未完成；Phase 040 只先证明代表 xyz AoS 点型 correctness。
- source-indexed、dual-indexed、correspondence 仍未接 scale RVV。
- `matrix-local-scale-simplification` 仍只是 weak-positive implementation-shape diagnostic；当前不再作为较小 patch 主线。
- `Scalar=double` 需要独立数值预算。

## 下一步

进入 `040-generic-point-type-expansion`：先补 `PointXYZI` / `PointXYZRGB` 等代表点型的 production public correctness（生产公开路径正确性）和 traits gate 证据，再决定是否继续做 generic point type board repeated / ASM attribution。
