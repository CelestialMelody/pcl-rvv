# transformation_estimation_dual_quaternion board repeated summary

## 运行合同

- run_label：`production_public_retained_row_sources_repeated`
- runs：`run-01, run-02, run-03, run-04, run-05`
- device：`Milkv-Jupiter`
- case-filter：`production-public-retained-row-sources`
- evidence_role：`production_direct`；当前摘要只覆盖 `ordered-cloud-pair` public entry，其它 row-source policy 由同一 run 的独立 manifest 分别登记。
- iterations：`20`
- warm-up iterations：`5`
- B/A：`Std public ordered-cloud-pair ms / RVV public ordered-cloud-pair ms`，大于 1 表示 RVV public entry 更快。
- manifest（默认本地证据清单）：`log/board/production_public_retained_row_sources_repeated/evidence_manifest.json`
- Evidence Doctor：`log/board/production_public_retained_row_sources_repeated/evidence_doctor.md`

## production public ordered-cloud-pair Std/RVV（同边界）

这一表比较同一 production ordered-cloud-pair public entry 的 Std 与 RVV 构建。RVV 构建命中当前 production dispatch 时，它用于判断该 row-source policy 是否满足生产证据门槛。

| size | values | median | min | max | bucket | checksum |
| --- | --- | ---: | ---: | ---: | --- | --- |
| 4K | `3.245, 3.355, 3.216, 3.232, 3.215` | 3.232 | 3.215 | 3.355 | `positive` | same |
| 64K | `3.657, 3.644, 3.645, 3.639, 3.636` | 3.644 | 3.636 | 3.657 | `positive` | same |
| 256K | `3.656, 3.656, 3.654, 3.660, 3.654` | 3.656 | 3.654 | 3.660 | `positive` | same |

## 决策桶

- overall_decision_bucket：`positive`（默认按 64K / 256K 等非 4K 目标规模判断；4K 保留为小规模诊断）。
- `positive` / `weak_positive` 只能按当前 mode 的证据边界解释；indexed / correspondences 不从 ordered-cloud-pair 自动继承。
- 任一 size 若为 `negative` 或 Evidence Doctor 有 Error，不能把该 row-source policy 纳入 production gate。
