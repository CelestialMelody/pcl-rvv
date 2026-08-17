# Board repeated summary: negative

## 运行上下文

- run_label: `production_direct_repeated`
- evidence_role: `strict_ab`
- device: `RISC-V board / target hardware`
- iterations: `50`
- warmup_iterations: `5`
- run_count: `5`
- decision_bucket: `negative`
- binary_hash: `std=sha256:628474b27a6c53f4b144cc1797b81efae8f410c228a079e8c43166b1f3e2899b; rvv=sha256:982d941d1a10dcfbb341cae3395efae74253902a8483b96a8f0fafeb112ae025`

## 结果

| case | family | size | runs | min | median | max | speedup_values | degradation_frequency | checksum |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| production-direct public entry 2048 correspondences | `production_edge_batch_rvv` | 2048 | 5 | 0.888x | 0.904x | 0.924x | 0.901x, 0.924x, 0.904x, 0.913x, 0.888x | 5/5 | match |
| production-direct public entry 8192 correspondences | `production_edge_batch_rvv` | 8192 | 5 | 0.892x | 0.977x | 0.984x | 0.892x, 0.977x, 0.984x, 0.971x, 0.977x | 5/5 | match |

## 证据边界

- 这是 board / target hardware（板卡 / 目标硬件）上的 production-direct（真实生产入口）重复测试；重新采集前必须确认当前 production patch 是否存在。
- `speedup = std_ms / rvv_ms`，大于 1 表示 RVV build 更快。
- 该 target 不修改 production 源码；summary 的证据角色取决于运行时的 production diff。有 Standard / RVV dispatch 时才可按 production RVV evidence（生产 RVV 证据）审查。
- summary、manifest 和 Evidence Doctor 是 summary artifact（摘要证据产物）。各 run 目录下的 `run_bench_*.log` 是 raw log（原始日志），默认不进入提交边界。
