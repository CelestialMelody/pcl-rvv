# transformation_estimation_svd production direct source-indexed-cloud-pair board repeated summary

## 运行合同

- run_label：`production_source_indexed_cloud_pair_repeated`
- runs：`run-01, run-02, run-03, run-04, run-05`
- device：`RVV board`
- case-filter：`source-indexed-cloud-pair`
- evidence_role：`production_direct`
- iterations：`20`
- warm-up iterations：`5`
- B/A：`baseline ms / candidate ms`，大于 1 表示 candidate 更快。
- manifest（默认本地证据清单）：`log/board/production_source_indexed_cloud_pair_repeated/evidence_manifest.json`
- Evidence Doctor：`log/board/production_source_indexed_cloud_pair_repeated/evidence_doctor.md`

## public Std/RVV（生产直连）

这一表只使用真实 public Umeyama 公开入口。Std build 是 baseline，RVV build 在满足 gate 时命中 production source-indexed-cloud-pair RVV 分流；不覆盖其它 row source 或 `Scalar=double`。

| size | values | median | min | max | bucket |
| --- | --- | ---: | ---: | ---: | --- |
| 4K | `9.524, 9.582, 9.910, 9.867, 9.634` | 9.634 | 9.524 | 9.910 | `positive` |
| 64K | `12.217, 12.181, 12.241, 11.881, 12.403` | 12.217 | 11.881 | 12.403 | `positive` |
| 256K | `11.759, 11.384, 11.443, 11.678, 11.558` | 11.558 | 11.384 | 11.759 | `positive` |

## 决策桶

- overall_decision_bucket：`positive`
- `positive` / `weak_positive` 只批准当前 gate 下的 dense source-indexed-cloud-pair、`Scalar=float`、xyz AoS 生产路径。
- 未逐项上板的 gate-allowed 点型只继承 correctness 与代表性性能判断，不能写成逐类型性能已证明。
- dual-indices、correspondences 和 `Scalar=double` 本摘要均不接 production。
