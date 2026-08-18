# transformation_estimation_dual_quaternion correspondence point-type layout summary

## 运行合同

- run_label：`correspondence_point_type_layout_repeated`
- device：`Milkv-Jupiter`
- case-filter：`correspondence-point-type-layout`
- evidence_role：`diagnostic`；只覆盖 correspondence-pair direct index stream 的代表性点型，不是 public-entry direct。
- B/A：`Std direct index stream ms / RVV direct index stream ms`，大于 1 表示 RVV 构建更快。
- manifest：`log/board/correspondence_point_type_layout_repeated/evidence_manifest.json`
- Evidence Doctor：`log/board/correspondence_point_type_layout_repeated/evidence_doctor.md`

## Point Type / Layout

| point type | size | values | median | min | max | bucket | path fingerprint |
| --- | --- | --- | ---: | ---: | ---: | --- | --- |
| PointXYZI | 4K | `5.175, 5.803, 5.494, 5.066, 5.523` | 5.494 | 5.066 | 5.803 | `positive` | Std `13504791834394936957` / RVV `1714001059226217133` |
| PointXYZI | 64K | `5.883, 6.685, 6.925, 6.687, 6.936` | 6.687 | 5.883 | 6.936 | `positive` | same |
| PointXYZI | 256K | `5.063, 5.764, 6.573, 4.041, 6.679` | 5.764 | 4.041 | 6.679 | `positive` | Std `1664452477084261135` / RVV `1706194153110589835` |
| PointXYZRGB | 4K | `5.675, 5.719, 5.718, 5.760, 5.770` | 5.719 | 5.675 | 5.770 | `positive` | same |
| PointXYZRGB | 64K | `6.927, 5.285, 6.138, 5.351, 6.523` | 6.138 | 5.285 | 6.927 | `positive` | same |
| PointXYZRGB | 256K | `2.960, 4.285, 4.119, 5.388, 5.744` | 4.285 | 2.960 | 5.744 | `positive` | Std `6877199880137518517` / RVV `4832430135946210872` |

## 决策边界

- overall_decision_bucket：`positive`。
- path fingerprint 若不同，只表示 bench 输出哈希不同；correctness 仍以 gtest 矩阵误差预算为准。
- 本摘要不能证明 public-entry dispatch、混合 source/target 点型或真实 workload 分布。
