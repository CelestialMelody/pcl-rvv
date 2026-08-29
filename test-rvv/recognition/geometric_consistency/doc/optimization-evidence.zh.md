# Optimization Evidence

| candidate family | current status | evidence | boundary |
| --- | --- | --- | --- |
| `pairwise-consistency-rvv` | adopted narrow production behavior | `run_test_compare`、`run_upstream_test_compare`、`check_gc_rvv_asm`、board proxy | 只覆盖局部 predicate |
| `cluster-growth-rvv` | positive diagnostic, deferred for later production expansion | `phase010_cluster_growth_diagnostic` + `phase020_cluster_growth_production_probe` board repeated | 仍需要更宽生产直连边界，当前没有新的 production helper |
| `production patch` | adopted narrow production behavior | 生产源码已接入 RVV helper，且 upstream test compare 通过 | 仍保留标量 fallback 和后续扩展空间 |
