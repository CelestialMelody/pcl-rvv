# transformation_estimation_dual_quaternion indexed direct gather point-type layout summary

## 运行合同

- run_label：`indexed_direct_gather_point_type_layout_repeated`
- device：`Milkv-Jupiter`
- case-filter：`indexed-direct-gather-point-type-layout`
- evidence_role：`diagnostic`；只覆盖 source-indexed / dual-indexed direct gather 的代表性点型，不是 public-entry direct。
- B/A：`Std direct gather ms / RVV direct gather ms`，大于 1 表示 RVV 构建更快。
- manifest：`log/board/indexed_direct_gather_point_type_layout_repeated/evidence_manifest.json`
- Evidence Doctor：`log/board/indexed_direct_gather_point_type_layout_repeated/evidence_doctor.md`

## Point Type / Layout

| row source | point type | size | values | median | min | max | bucket | path fingerprint |
| --- | --- | --- | --- | ---: | ---: | ---: | --- | --- |
| dual-indexed-cloud-pair | PointXYZI | 4K | `2.370, 2.337, 2.422, 2.241, 2.375` | 2.370 | 2.241 | 2.422 | `positive` | Std `13504791834394936957` / RVV `1714001059226217133` |
| dual-indexed-cloud-pair | PointXYZI | 64K | `2.920, 3.034, 2.954, 3.126, 3.084` | 3.034 | 2.920 | 3.126 | `positive` | same |
| dual-indexed-cloud-pair | PointXYZI | 256K | `4.685, 4.628, 4.673, 4.610, 4.520` | 4.628 | 4.520 | 4.685 | `positive` | Std `1664452477084261135` / RVV `1706194153110589835` |
| dual-indexed-cloud-pair | PointXYZRGB | 4K | `2.486, 2.524, 2.428, 2.464, 2.137` | 2.464 | 2.137 | 2.524 | `positive` | same |
| dual-indexed-cloud-pair | PointXYZRGB | 64K | `2.982, 3.115, 2.834, 3.028, 3.147` | 3.028 | 2.834 | 3.147 | `positive` | same |
| dual-indexed-cloud-pair | PointXYZRGB | 256K | `4.814, 4.866, 4.850, 4.862, 4.950` | 4.862 | 4.814 | 4.950 | `positive` | Std `6877199880137518517` / RVV `4832430135946210872` |
| source-indexed-cloud-pair | PointXYZI | 4K | `2.474, 2.639, 2.444, 2.409, 2.420` | 2.444 | 2.409 | 2.639 | `positive` | Std `13504791834394936957` / RVV `1714001059226217133` |
| source-indexed-cloud-pair | PointXYZI | 64K | `2.800, 2.781, 2.798, 2.773, 2.800` | 2.798 | 2.773 | 2.800 | `positive` | same |
| source-indexed-cloud-pair | PointXYZI | 256K | `2.845, 2.846, 2.887, 2.862, 2.817` | 2.846 | 2.817 | 2.887 | `positive` | Std `1664452477084261135` / RVV `1706194153110589835` |
| source-indexed-cloud-pair | PointXYZRGB | 4K | `2.608, 2.483, 2.476, 2.492, 2.400` | 2.483 | 2.400 | 2.608 | `positive` | same |
| source-indexed-cloud-pair | PointXYZRGB | 64K | `2.875, 2.835, 2.950, 2.802, 2.866` | 2.866 | 2.802 | 2.950 | `positive` | same |
| source-indexed-cloud-pair | PointXYZRGB | 256K | `2.845, 2.840, 2.827, 2.869, 2.896` | 2.845 | 2.827 | 2.896 | `positive` | Std `6877199880137518517` / RVV `4832430135946210872` |

## 决策边界

- overall_decision_bucket：`positive`。
- path fingerprint 若不同，只表示 bench 输出哈希不同；correctness 仍以 gtest 矩阵误差预算为准。
- 本摘要不能证明 public-entry dispatch、correspondence-pair direct index stream 或真实 workload 分布。
