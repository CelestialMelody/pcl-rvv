# transformation_estimation_2D Board Repeated Summary

## 运行合同

- evidence_role：post-production public dispatch（接入生产后公开入口证据）
- run_label：`ordered_cloud_pair_public_repeated`
- case_filter：`ordered-cloud-pair-public`
- expected_runs：5
- device：Milkv-Jupiter

`B/A = Std_ms / RVV_ms`，大于 1 表示 RVV build 更快。
本摘要用于 PI4 production public evidence；QEMU timing 仍不参与性能结论。

## 结果

| case | runs | median | min | max | p10 | p90 | B/A<1 | bucket | values |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- | --- |
| `public 2D ordered-cloud-pair 4K` | 5 | 4.222x | 4.110x | 4.231x | 4.147x | 4.231x | 0 | `positive` | 4.222x, 4.231x, 4.203x, 4.110x, 4.231x |
| `public 2D ordered-cloud-pair 64K` | 5 | 5.310x | 5.199x | 5.362x | 5.215x | 5.352x | 0 | `positive` | 5.240x, 5.310x, 5.362x, 5.199x, 5.338x |
| `public 2D ordered-cloud-pair 256K` | 5 | 4.947x | 4.864x | 5.068x | 4.894x | 5.031x | 0 | `positive` | 5.068x, 4.940x, 4.974x, 4.864x, 4.947x |

## Checksum 边界

bench log checksum 包含 RVV gate flag（路径标记），因此 Std / RVV log checksum 不要求相同。
数值正确性主证据仍是 `run_test_compare` 和 near-cancellation correctness corpus。
