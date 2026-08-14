# transformation_estimation_svd production direct correspondence-pair board repeated summary

## 运行合同

- run_label：`production_correspondence_pair_repeated`
- runs：`run-01, run-02, run-03, run-04, run-05`
- device：`RVV board`
- case-filter：`correspondence-pair`
- evidence_role：`production_direct`
- iterations：`20`
- warm-up iterations：`5`
- B/A：`baseline ms / candidate ms`，大于 1 表示 candidate 更快。
- manifest（默认本地证据清单）：`log/board/production_correspondence_pair_repeated/evidence_manifest.json`
- Evidence Doctor：`log/board/production_correspondence_pair_repeated/evidence_doctor.md`

## public Std/RVV（生产直连）

这一表只使用真实 public Umeyama 公开入口。Std build 是 baseline，RVV build 在满足 gate 时命中 production correspondence-pair RVV 分流；不覆盖其它 row source 或 `Scalar=double`。

| size | values | median | min | max | bucket |
| --- | --- | ---: | ---: | ---: | --- |
| 4K | `8.427, 8.317, 8.649, 8.905, 9.106` | 8.649 | 8.317 | 9.106 | `positive` |
| 64K | `8.302, 8.646, 8.644, 8.618, 8.872` | 8.644 | 8.302 | 8.872 | `positive` |
| 256K | `8.305, 8.181, 7.006, 7.800, 7.872` | 7.872 | 7.006 | 8.305 | `positive` |

## 决策桶

- overall_decision_bucket：`positive`
- `positive` / `weak_positive` 只批准当前 gate 下的 dense correspondence-pair、`Scalar=float`、xyz AoS 生产路径。
- 未逐项上板的 gate-allowed 点型只继承 correctness 与代表性性能判断，不能写成逐类型性能已证明。
- none 和 `Scalar=double` 本摘要均不接 production。
