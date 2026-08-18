# transformation_estimation_dual_quaternion source-indexed family summary

## 运行合同

- run_label：`row_source_family_comparison_repeated`
- runs：`run-01, run-02, run-03, run-04, run-05`
- device：`Milkv-Jupiter`
- case-filter：`row-source-family-comparison`
- evidence_role：`diagnostic`；这是同一 RVV 二进制内的实现族比较，不是 production direct。
- iterations：`20`
- warm-up iterations：`5`
- B/A：`staged ordered reuse RVV ms / direct indexed gather RVV ms`，大于 1 表示 direct gather 更快。
- manifest：`log/board/row_source_family_comparison_repeated/evidence_manifest.json`
- Evidence Doctor：`log/board/row_source_family_comparison_repeated/evidence_doctor.md`

## source-indexed staged-vs-direct RVV family

两侧使用同一 RVV binary、source-indexed row pairing、C1/C2 公式、Eigen 4x4 solve 和 checksum policy；唯一目标差异是 source ingress：staged candidate 先 materialize，direct candidate 使用三次 `vluxei32` 读取 source x/y/z。索引展开、gather、累加和 solve 均在候选计时边界内。

| size | values | median | min | max | bucket | checksum |
| --- | --- | ---: | ---: | ---: | --- | --- |
| 256K | `1.505, 1.470, 1.468, 1.471, 1.458` | 1.470 | 1.458 | 1.505 | `positive` | same |
| 4K | `1.254, 1.243, 1.242, 1.231, 1.237` | 1.242 | 1.231 | 1.254 | `positive` | same |
| 64K | `1.481, 1.442, 1.454, 1.479, 1.458` | 1.458 | 1.442 | 1.481 | `positive` | same |

## 决策桶

- overall_decision_bucket：`positive`（默认按 64K / 256K 判断，4K 保留为小规模诊断）。
- positive / weak_positive 只表示 test-rvv implementation-family comparison 的结果，不能升级为 production-ready。
- 若 direct gather 的收益方向不稳定，必须保留 staged candidate，并把 family comparison 降级为 unstable 或 neutral。
