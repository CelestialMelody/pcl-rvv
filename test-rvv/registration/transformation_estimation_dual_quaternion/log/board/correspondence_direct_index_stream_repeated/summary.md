# transformation_estimation_dual_quaternion correspondence direct index-stream summary

## 运行合同

- run_label：`correspondence_direct_index_stream_repeated`
- runs：`run-01, run-02, run-03, run-04, run-05`
- device：`Milkv-Jupiter`
- case-filter：`correspondence-direct-index-stream-comparison`
- evidence_role：`diagnostic`；只覆盖 `correspondence-pair`，不是 production direct。
- iterations：`20`
- warm-up iterations：`5`
- B/A：各比较均为 baseline ms / candidate ms，大于 1 表示 candidate 更快。
- 三侧共享 RVV binary、C1/C2 公式、Eigen 4x4 solve 和 checksum policy；差异只在 correspondence row ingress。
- manifest：`log/board/correspondence_direct_index_stream_repeated/evidence_manifest.json`
- Evidence Doctor：`log/board/correspondence_direct_index_stream_repeated/evidence_doctor.md`

## correspondence-pair 256K

| comparison | values | median | min | max | bucket |
| --- | --- | ---: | ---: | ---: | --- |
| staged ordered reuse / direct indexed gather | `1.752, 1.663, 1.064, 1.702, 1.630` | 1.663 | 1.064 | 1.752 | `weak_positive` |
| staged ordered reuse / direct index stream | `2.527, 1.852, 1.351, 3.089, 2.779` | 2.527 | 1.351 | 3.089 | `positive` |
| direct indexed gather / direct index stream | `1.442, 1.114, 1.270, 1.815, 1.705` | 1.442 | 1.114 | 1.815 | `weak_positive` |
- checksum：same

## correspondence-pair 4K

| comparison | values | median | min | max | bucket |
| --- | --- | ---: | ---: | ---: | --- |
| staged ordered reuse / direct indexed gather | `1.591, 1.565, 1.565, 1.554, 1.605` | 1.565 | 1.554 | 1.605 | `positive` |
| staged ordered reuse / direct index stream | `2.505, 2.356, 2.461, 2.525, 2.529` | 2.505 | 2.356 | 2.529 | `positive` |
| direct indexed gather / direct index stream | `1.575, 1.506, 1.573, 1.625, 1.576` | 1.575 | 1.506 | 1.625 | `positive` |
- checksum：same

## correspondence-pair 64K

| comparison | values | median | min | max | bucket |
| --- | --- | ---: | ---: | ---: | --- |
| staged ordered reuse / direct indexed gather | `2.054, 1.832, 1.717, 1.834, 1.685` | 1.832 | 1.685 | 2.054 | `positive` |
| staged ordered reuse / direct index stream | `3.169, 3.076, 3.104, 2.954, 2.953` | 3.076 | 2.953 | 3.169 | `positive` |
| direct indexed gather / direct index stream | `1.543, 1.679, 1.808, 1.610, 1.752` | 1.679 | 1.543 | 1.808 | `positive` |
- checksum：same

## 决策边界

- staged-vs-direct overall：`weak_positive`。
- staged-vs-index-stream overall：`positive`。
- direct-gather-vs-index-stream overall：`weak_positive`。
- 64K 若继续偏离 4K / 256K，只按 correspondence 的 size 独立解释；不能单因归因于 gather 或 AoS 展开。
- direct index stream 若收益不稳定，保留 Phase 007 direct gather 作为明确 baseline，不升级为默认路径。
- 所有正向结果仍是 test-rvv 诊断证据，不能写成 production-ready。