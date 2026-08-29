# Benchmark and Evidence

bench 先度量固定 consensus set 上的 batch predicate，再保留生产直连 correctness 作为
`recognize()` 入口的独立证据。不把 diagnostic bench 直接写成 production direct。

| case | 入口 | 计时边界 | 证明点 | 不能证明 |
| --- | --- | --- | --- | --- |
| `pairwise_consistency_batch` | `bench_gc` | predicate batch + checksum | 局部算术段是否值得继续 | 不能直接外推到 production |
| `cluster_growth` | `bench_gc` + `growth` mode | full growth helper + checksum | outer growth 形态是否也继续正向 | 不能直接外推到新的 production boundary |

证据文件默认走 `log/board/repeated_*`、`evidence_manifest.json`、`evidence_doctor.md`
和 `evidence_registry.json`。

## 本阶段证据

- `test-rvv/recognition/geometric_consistency/log/board/repeated_phase000_pairwise_consistency_diagnostic/summary.md`
- `test-rvv/recognition/geometric_consistency/log/board/repeated_phase000_pairwise_consistency_diagnostic/evidence_manifest.json`
- `test-rvv/recognition/geometric_consistency/log/board/repeated_phase000_pairwise_consistency_diagnostic/evidence_doctor.md`
- `test-rvv/recognition/geometric_consistency/log/board/repeated_phase000_pairwise_consistency_diagnostic/evidence_doctor.json`
- `test-rvv/recognition/geometric_consistency/log/board/repeated_phase010_cluster_growth_diagnostic/summary.md`
- `test-rvv/recognition/geometric_consistency/log/board/repeated_phase010_cluster_growth_diagnostic/evidence_manifest.json`
- `test-rvv/recognition/geometric_consistency/log/board/repeated_phase010_cluster_growth_diagnostic/evidence_doctor.md`
- `test-rvv/recognition/geometric_consistency/log/board/repeated_phase010_cluster_growth_diagnostic/evidence_doctor.json`
- `test-rvv/recognition/geometric_consistency/log/board/repeated_phase020_cluster_growth_production_probe/summary.md`
- `test-rvv/recognition/geometric_consistency/log/board/repeated_phase020_cluster_growth_production_probe/evidence_manifest.json`
- `test-rvv/recognition/geometric_consistency/log/board/repeated_phase020_cluster_growth_production_probe/evidence_doctor.md`
- `test-rvv/recognition/geometric_consistency/log/board/repeated_phase020_cluster_growth_production_probe/evidence_doctor.json`
- `test-rvv/recognition/geometric_consistency/log/evidence_registry.json`
