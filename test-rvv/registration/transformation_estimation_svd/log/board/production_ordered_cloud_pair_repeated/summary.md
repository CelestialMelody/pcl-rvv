# transformation_estimation_svd production direct ordered-cloud-pair board repeated summary

## 运行合同

- run_label：`production_ordered_cloud_pair_repeated`
- runs：`run-01, run-02, run-03, run-04, run-05`
- device：`RVV board`
- case-filter：`public-umeyama`
- evidence_role：`production_direct`
- iterations：`20`
- warm-up iterations：`5`
- B/A：`baseline ms / candidate ms`，大于 1 表示 candidate 更快。
- manifest（默认本地证据清单）：`log/board/production_ordered_cloud_pair_repeated/evidence_manifest.json`
- Evidence Doctor：`log/board/production_ordered_cloud_pair_repeated/evidence_doctor.md`

## public Std/RVV（生产直连）

这一表只使用真实 public Umeyama 公开入口。Std build 是 baseline，RVV build 在满足 gate 时命中 production ordered-cloud-pair RVV 分流；不覆盖 indices、correspondences 或 `Scalar=double`。

| size | values | median | min | max | bucket |
| --- | --- | ---: | ---: | ---: | --- |
| 4K | `14.508, 14.146, 14.514, 14.256, 14.372` | 14.372 | 14.146 | 14.514 | `positive` |
| 64K | `24.236, 24.534, 24.745, 24.471, 23.830` | 24.471 | 23.830 | 24.745 | `positive` |
| 256K | `23.841, 24.299, 23.814, 24.490, 23.645` | 23.841 | 23.645 | 24.490 | `positive` |

## 决策桶

- overall_decision_bucket：`positive`
- `positive` / `weak_positive` 只批准当前 gate 下的 dense ordered-cloud-pair、`Scalar=float`、xyz AoS 生产路径。
- 未逐项上板的 gate-allowed 点型只继承 correctness 与代表性性能判断，不能写成逐类型性能已证明。
- indices、dual-indices、correspondences 和 `Scalar=double` 本摘要均不接 production。
