# bfgs Board Direction-Update Repeated Summary

## 运行合同

- evidence_role：pre-production diagnostic（接入生产前诊断）
- run_label：`direction_update_repeated`
- case_filter：`direction-update`
- expected_runs：5
- device：Milkv-Jupiter

`B/A = Std_ms / RVV_ms`，大于 1 表示 RVV build 更快。
本摘要不证明 production dispatch；它只回答 test-only direction-update 是否值得继续进入 caller hotspot audit。

## 结果

| case | size | runs | median | min | max | p10 | p90 | B/A<1 | bucket | values |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- | --- |
| `direction-update-vector6` | 6 | 5 | 0.648x | 0.638x | 0.687x | 0.639x | 0.678x | 5 | `negative` | 0.648x, 0.640x, 0.664x, 0.638x, 0.687x |
| `direction-update-vector128` | 128 | 5 | 0.740x | 0.734x | 0.770x | 0.736x | 0.769x | 5 | `negative` | 0.739x, 0.734x, 0.769x, 0.740x, 0.770x |

## Checksum 边界

bench log checksum 只覆盖计算结果，不再混入 `used_rvv` 路径标志。
由于 double reduction（双精度归约）顺序可能导致 bit-level（逐位）差异，exact log checksum 只作为 diagnostic sample；数值正确性主证据仍是 `run_test_compare`。

| case | Std checksum sample | RVV checksum sample | exact match |
| --- | --- | --- | --- |
| `direction-update-vector6` | `3255807657299441210` | `968818922218053737` | `False` |
| `direction-update-vector128` | `9009593913952823174` | `1688574400692744509` | `False` |

## 阶段解释

- primary_decision_bucket：`negative`（优先按 `direction-update-vector6` 判断，因为它接近 GICP 状态维度）。
- `direction-update-vector128` 只检查 VL chunk 形态，不代表当前 production caller 工作集。
- 当前不支持继续 production 接入路径；除非用户明确要求做 bounded negative-analysis，否则 topic 应停在 diagnostic closeout。
