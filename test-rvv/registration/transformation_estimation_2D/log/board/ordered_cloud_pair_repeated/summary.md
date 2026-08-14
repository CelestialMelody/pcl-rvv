# transformation_estimation_2D Board Repeated Summary

## 运行合同

- evidence_role：pre-production diagnostic（接入生产前诊断）
- run_label：`ordered_cloud_pair_repeated`
- case_filter：`ordered-cloud-pair-fused`
- expected_runs：5
- device：Milkv-Jupiter

`B/A = Std_ms / RVV_ms`，大于 1 表示 RVV build 更快。
本摘要不证明 production dispatch；production 接入需要 PI1-PI5 证据闭环。

## 结果

| case | runs | median | min | max | p10 | p90 | B/A<1 | bucket | values |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- | --- |
| `fused 2D correlation ordered-cloud-pair 4K` | 5 | 1.176x | 1.166x | 1.191x | 1.167x | 1.185x | 0 | `positive` | 1.166x, 1.191x, 1.168x, 1.177x, 1.176x |
| `fused 2D correlation ordered-cloud-pair 64K` | 5 | 1.096x | 1.068x | 1.145x | 1.078x | 1.132x | 0 | `weak_positive` | 1.068x, 1.093x, 1.113x, 1.145x, 1.096x |
| `fused 2D correlation ordered-cloud-pair 256K` | 5 | 1.096x | 1.080x | 1.121x | 1.083x | 1.112x | 0 | `weak_positive` | 1.089x, 1.121x, 1.099x, 1.096x, 1.080x |

## Checksum 边界

bench log checksum 包含 RVV gate flag（路径标记），因此 Std / RVV log checksum 不要求相同。
数值正确性主证据仍是 `run_test_compare` 和 near-cancellation correctness corpus。
