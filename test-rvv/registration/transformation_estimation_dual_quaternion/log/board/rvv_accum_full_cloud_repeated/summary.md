# transformation_estimation_dual_quaternion board repeated summary

## 运行合同

- run_label：`rvv_accum_full_cloud_repeated`
- runs：`run-01, run-02, run-03, run-04, run-05`
- device：`Milkv-Jupiter`
- case-filter：`rvv-accum-full-cloud`
- evidence_role：`diagnostic`；同边界 Std/RVV helper 对比为 `strict_ab`，但不是 production direct（真实生产路径证据）。
- iterations：`20`
- warm-up iterations：`5`
- B/A：`Std helper ms / RVV helper ms`，大于 1 表示 RVV helper 更快。
- manifest（默认本地证据清单）：`log/board/rvv_accum_full_cloud_repeated/evidence_manifest.json`
- Evidence Doctor：`log/board/rvv_accum_full_cloud_repeated/evidence_doctor.md`

## rvv accum Std/RVV（同边界）

这一表只比较同一 test-support ordered full-cloud helper 的 Std 与 RVV 构建，用于判断 TEDQ C1/C2 accumulation candidate 是否值得进入 PI1。

| size | values | median | min | max | bucket | checksum |
| --- | --- | ---: | ---: | ---: | --- | --- |
| 4K | `1.912, 2.039, 2.014, 2.031, 1.984` | 2.014 | 1.912 | 2.039 | `positive` | same |
| 64K | `2.066, 2.060, 2.108, 2.094, 1.994` | 2.066 | 1.994 | 2.108 | `positive` | same |
| 256K | `2.082, 2.097, 2.122, 2.105, 2.015` | 2.097 | 2.015 | 2.122 | `positive` | same |

## 决策桶

- overall_decision_bucket：`positive`（默认按 64K / 256K 等非 4K 目标规模判断；4K 保留为小规模诊断）。
- `positive` / `weak_positive` 只能支持进入 PI1；仍需 production direct tests、fallback matrix 和 production bench。
- 任一 size 若为 `negative` 或 Evidence Doctor 有 Error，不能把该 size 纳入 production gate。
