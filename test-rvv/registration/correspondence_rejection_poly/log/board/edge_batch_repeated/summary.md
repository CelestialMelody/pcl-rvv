# Board repeated summary: weak_positive

## 运行上下文

- run_label: `edge_batch_repeated`
- evidence_role: `strict_ab`
- device: `RISC-V board / target hardware`
- iterations: `50`
- warmup_iterations: `5`
- run_count: `5`
- decision_bucket: `weak_positive`
- binary_hash: `std=sha256:63c6fce3be45d131ee0e6b7ad6a343898d6f51ccad1865e469ef80a62a88cd80; rvv=sha256:3be04c7843b044d2c59d54c5e375812bccededd02e096bd96b633368c2948f3f`

## 结果

| case | family | size | runs | min | median | max | speedup_values | degradation_frequency | checksum |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| edge-batch candidate 256K | `edge_length_batch` | 262144 | 5 | 1.027x | 1.057x | 1.084x | 1.069x, 1.084x, 1.057x, 1.027x, 1.029x | 0/5 | match |
| edge-batch candidate 64K | `edge_length_batch` | 65536 | 5 | 1.066x | 1.069x | 1.096x | 1.066x, 1.069x, 1.096x, 1.066x, 1.078x | 0/5 | match |

## 证据边界

- 这是 board / target hardware（板卡 / 目标硬件）上的 test support candidate wrapper（测试支撑候选包装）重复测试。
- `speedup = std_ms / rvv_ms`，大于 1 表示 RVV build 更快。
- `full_entry_diagnostic` 若出现，只证明真实 public entry（公开入口）标量完整调用可运行；当前没有 production RVV dispatch。
- summary、manifest 和 Evidence Doctor 是 summary artifact（摘要证据产物）。各 run 目录下的 `run_bench_*.log` 是 raw log（原始日志），默认不进入提交边界。
