# Board repeated summary: negative

## 运行上下文

- run_label: `production_direct_repeated`
- evidence_role: `strict_ab`
- device: `RISC-V board / target hardware`
- iterations: `50`
- warmup_iterations: `5`
- run_count: `5`
- decision_bucket: `negative`
- binary_hash: `std=sha256:b8f26921e994258a0d94e58388e6f49d44ce9d0a216ad6c38f36d80bd50b4878; rvv=sha256:90a32c61366f6c7d1684df276b3277f200b410bb791b02e011af7cca4db692db`

## 结果

| case | family | size | runs | min | median | max | speedup_values | degradation_frequency | checksum |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| production-direct public entry 2048 correspondences | `production_edge_batch_rvv` | 2048 | 5 | 0.863x | 0.901x | 0.917x | 0.911x, 0.863x, 0.890x, 0.917x, 0.901x | 5/5 | match |
| production-direct public entry 8192 correspondences | `production_edge_batch_rvv` | 8192 | 5 | 0.941x | 0.956x | 0.968x | 0.958x, 0.941x, 0.955x, 0.968x, 0.956x | 5/5 | match |

## 证据边界

- 这是 board / target hardware（板卡 / 目标硬件）上的 historical production direct probe（历史真实生产入口探针）重复测试；只有临时生产补丁存在并显式设置 `CRPOLY_ENABLE_PRODUCTION_DIRECT_PROBE=1` 时才应重新采集。
- `speedup = std_ms / rvv_ms`，大于 1 表示 RVV build 更快。
- 当前 topic 的生产补丁已回滚；该 summary 只作为 rollback/no-production（回滚且不接入生产）的历史负向证据，不代表当前 production 源码仍有 RVV dispatch。
- summary、manifest 和 Evidence Doctor 是 summary artifact（摘要证据产物）。各 run 目录下的 `run_bench_*.log` 是 raw log（原始日志），默认不进入提交边界。
