# transformation_estimation_2D Board Repeated Summary

## 运行合同

- evidence_role：row-source diagnostic（行来源诊断）
- run_label：`row_source_fused_repeated`
- case_filter：`row-source-fused`
- expected_runs：5
- device：Milkv-Jupiter

`B/A = Std_ms / RVV_ms`，大于 1 表示 RVV build 更快。
本摘要包含 materialize-to-ordered 展开成本；它只用于 row-source 诊断，不证明 production dispatch。

## 结果

| case | runs | median | min | max | p10 | p90 | B/A<1 | bucket | values |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- | --- |
| `fused 2D correlation correspondence-pair 4K` | 5 | 1.067x | 1.016x | 1.123x | 1.035x | 1.106x | 0 | `weak_positive` | 1.063x, 1.067x, 1.080x, 1.016x, 1.123x |
| `fused 2D correlation correspondence-pair 64K` | 5 | 1.013x | 0.992x | 1.023x | 0.993x | 1.022x | 2 | `neutral` | 1.021x, 0.996x, 1.023x, 0.992x, 1.013x |
| `fused 2D correlation correspondence-pair 256K` | 5 | 1.035x | 1.025x | 1.063x | 1.028x | 1.056x | 0 | `weak_positive` | 1.035x, 1.025x, 1.046x, 1.033x, 1.063x |
| `fused 2D correlation dual-indexed-cloud-pair 4K` | 5 | 1.081x | 1.070x | 1.166x | 1.071x | 1.141x | 0 | `weak_positive` | 1.081x, 1.072x, 1.104x, 1.070x, 1.166x |
| `fused 2D correlation dual-indexed-cloud-pair 64K` | 5 | 1.014x | 0.956x | 1.114x | 0.976x | 1.083x | 1 | `negative` | 1.006x, 1.014x, 1.114x, 0.956x, 1.036x |
| `fused 2D correlation dual-indexed-cloud-pair 256K` | 5 | 1.038x | 1.003x | 1.104x | 1.016x | 1.089x | 0 | `weak_positive` | 1.038x, 1.003x, 1.067x, 1.035x, 1.104x |
| `fused 2D correlation source-indexed-cloud-pair 4K` | 5 | 1.073x | 1.072x | 1.082x | 1.072x | 1.081x | 0 | `weak_positive` | 1.072x, 1.079x, 1.072x, 1.082x, 1.073x |
| `fused 2D correlation source-indexed-cloud-pair 64K` | 5 | 1.023x | 1.012x | 1.062x | 1.014x | 1.050x | 0 | `unstable` | 1.023x, 1.012x, 1.032x, 1.016x, 1.062x |
| `fused 2D correlation source-indexed-cloud-pair 256K` | 5 | 1.038x | 1.032x | 1.077x | 1.033x | 1.064x | 0 | `weak_positive` | 1.038x, 1.032x, 1.046x, 1.034x, 1.077x |

## Checksum 边界

bench log checksum 包含 RVV gate flag（路径标记），因此 Std / RVV log checksum 不要求相同。
数值正确性主证据仍是 `run_test_compare` 和 near-cancellation correctness corpus。
