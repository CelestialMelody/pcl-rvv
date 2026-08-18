# transformation_estimation_dual_quaternion indexed direct-gather family summary

## 运行合同

- run_label：`indexed_direct_gather_family_comparison_repeated`
- runs：`run-01, run-02, run-03, run-04, run-05`
- device：`Milkv-Jupiter`
- case-filter：`indexed-direct-gather-family-comparison`
- evidence_role：`diagnostic`；三类 indexed policy 独立比较，不是 production direct。
- iterations：`20`
- warm-up iterations：`5`
- B/A：`staged ordered reuse RVV ms / direct indexed gather RVV ms`，大于 1 表示 direct gather 更快。
- manifest：`log/board/indexed_direct_gather_family_comparison_repeated/evidence_manifest.json`
- Evidence Doctor：`log/board/indexed_direct_gather_family_comparison_repeated/evidence_doctor.md`

## source-indexed-cloud-pair staged-vs-direct RVV family

两侧使用同一 RVV binary、同一 row source、同一 C1/C2 公式、同一 Eigen 4x4 solve 和同一 checksum policy；唯一目标差异是 indexed row ingress。

| size | values | median | min | max | bucket | checksum |
| --- | --- | ---: | ---: | ---: | --- | --- |
| 256K | `1.477, 1.461, 1.487, 1.454, 1.452` | 1.461 | 1.452 | 1.487 | `positive` | same |
| 4K | `1.296, 1.346, 1.263, 1.325, 1.290` | 1.296 | 1.263 | 1.346 | `positive` | same |
| 64K | `1.469, 1.437, 1.478, 1.481, 1.454` | 1.469 | 1.437 | 1.481 | `positive` | same |
- policy_decision_bucket：`positive`（默认按 64K / 256K 判断，4K 保留为小规模诊断）。

## dual-indexed-cloud-pair staged-vs-direct RVV family

两侧使用同一 RVV binary、同一 row source、同一 C1/C2 公式、同一 Eigen 4x4 solve 和同一 checksum policy；唯一目标差异是 indexed row ingress。

| size | values | median | min | max | bucket | checksum |
| --- | --- | ---: | ---: | ---: | --- | --- |
| 256K | `1.703, 1.726, 1.758, 1.709, 1.705` | 1.709 | 1.703 | 1.758 | `positive` | same |
| 4K | `1.893, 1.869, 1.901, 1.862, 1.892` | 1.892 | 1.862 | 1.901 | `positive` | same |
| 64K | `1.219, 1.316, 1.188, 1.296, 1.170` | 1.219 | 1.170 | 1.316 | `positive` | same |
- policy_decision_bucket：`positive`（默认按 64K / 256K 判断，4K 保留为小规模诊断）。

## correspondence-pair staged-vs-direct RVV family

两侧使用同一 RVV binary、同一 row source、同一 C1/C2 公式、同一 Eigen 4x4 solve 和同一 checksum policy；唯一目标差异是 indexed row ingress。

| size | values | median | min | max | bucket | checksum |
| --- | --- | ---: | ---: | ---: | --- | --- |
| 256K | `1.672, 1.567, 1.591, 1.620, 1.581` | 1.591 | 1.567 | 1.672 | `positive` | same |
| 4K | `1.469, 1.552, 1.626, 1.506, 1.570` | 1.552 | 1.469 | 1.626 | `positive` | same |
| 64K | `1.196, 1.119, 1.170, 1.188, 1.121` | 1.170 | 1.119 | 1.196 | `weak_positive` | same |
- policy_decision_bucket：`weak_positive`（默认按 64K / 256K 判断，4K 保留为小规模诊断）。

## 决策边界

- 每个 indexed policy 独立判断；不能把 source-indexed、dual-indexed 或 correspondence 的结果互相外推。
- positive / weak_positive 只表示 test-rvv implementation-family comparison，不能升级为 production-ready。
- 若 direct gather 的收益方向不稳定，必须保留 staged candidate，并把对应 policy 降级为 unstable 或 neutral。
