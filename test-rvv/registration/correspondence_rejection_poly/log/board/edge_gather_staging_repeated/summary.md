# Board repeated summary: weak_positive

## 运行上下文

- run_label: `edge_gather_staging_repeated`
- evidence_role: `strict_ab`
- device: `RISC-V board / target hardware`
- iterations: `50`
- warmup_iterations: `5`
- run_count: `5`
- decision_bucket: `weak_positive`
- binary_hash: `std=sha256:d5a5b7cf24086f2d9d9d19f754b763e4a7109c791f8e73f550681d8914d0f751; rvv=sha256:6ae121a87085d1cf0c61e47995bab934c597cd7a231d336c74e1b8b2776f1563`

## 结果

| case | family | size | runs | min | median | max | speedup_values | degradation_frequency | checksum |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| edge-gather-staging candidate 256K | `edge_gather_staging` | 262144 | 5 | 1.109x | 1.119x | 1.132x | 1.124x, 1.119x, 1.109x, 1.111x, 1.132x | 0/5 | match |
| edge-gather-staging candidate 64K | `edge_gather_staging` | 65536 | 5 | 1.095x | 1.100x | 1.116x | 1.104x, 1.116x, 1.100x, 1.099x, 1.095x | 0/5 | match |

## 证据边界

- 这是 board / target hardware（板卡 / 目标硬件）上的 test support candidate wrapper（测试支撑候选包装）重复测试。
- `speedup = std_ms / rvv_ms`，大于 1 表示 RVV build 更快。
- `full_entry_diagnostic` 若出现，只证明真实 public entry（公开入口）标量完整调用可运行；当前没有 production RVV dispatch。
- summary、manifest 和 Evidence Doctor 是 summary artifact（摘要证据产物）。各 run 目录下的 `run_bench_*.log` 是 raw log（原始日志），默认不进入提交边界。
