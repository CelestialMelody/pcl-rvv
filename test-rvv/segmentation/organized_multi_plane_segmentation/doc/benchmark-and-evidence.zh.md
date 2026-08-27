# Benchmark And Evidence

## Bench Case 字典

| case-filter | 证据角色 | 计时边界 | 当前结论 |
| --- | --- | --- | --- |
| `plane_d_dot` | diagnostic component ablation | 只计 `plane_d` 点积 helper | negative，median 0.81x |
| `boundary_gather` | diagnostic component ablation | 只计 boundary gather helper | neutral，median 1.03x |
| `projection` | diagnostic component ablation | 只计 viewpoint projection helper | positive，median 1.34x |
| `region_projected` | production-shaped diagnostic | 多 region gather + projection + checksum | negative，median 0.89x |
| `region_gather_only` | production-shaped diagnostic | 多 region gather + checksum | negative，median 0.80x |

## 当前证据路径

| run label | summary | manifest | Evidence Doctor |
| --- | --- | --- | --- |
| `phase000-board-repeated-5x` | `log/board/repeated/summary.md` | `log/board/repeated/evidence_manifest.json` | `log/board/repeated/evidence_doctor.md` |
| `phase010-region_projected-5x` | `log/board/phase010-region_projected/summary.md` | `log/board/phase010-region_projected/evidence_manifest.json` | `log/board/phase010-region_projected/evidence_doctor.md` |
| `phase010-region_gather_only-5x` | `log/board/phase010-region_gather_only/summary.md` | `log/board/phase010-region_gather_only/evidence_manifest.json` | `log/board/phase010-region_gather_only/evidence_doctor.md` |

## 复现命令

```bash
make -C test-rvv/segmentation/organized_multi_plane_segmentation run_test_compare
make -C test-rvv/segmentation/organized_multi_plane_segmentation dump_bench_rvv
make -C test-rvv/segmentation/organized_multi_plane_segmentation run_board_omps_repeated
make -C test-rvv/segmentation/organized_multi_plane_segmentation run_board_omps_repeated \
  OMPS_REPEATED_DIR=log/board/phase010-region_projected \
  OMPS_BENCH_ARGS='--size 262144 --iterations 8 --warmup 2 --case-filter region_projected'
make -C test-rvv/segmentation/organized_multi_plane_segmentation run_board_omps_repeated \
  OMPS_REPEATED_DIR=log/board/phase010-region_gather_only \
  OMPS_BENCH_ARGS='--size 262144 --iterations 8 --warmup 2 --case-filter region_gather_only'
```

## Evidence Policy

`build/`、QEMU bench timing 和 raw per-run board logs 默认不提交。被本文件、phase result、evaluation 和 Handoff 引用的 summary / manifest / Evidence Doctor 可作为 summary evidence（摘要证据）候选。`log/evidence_registry.json` 记录这些摘要证据的新鲜度。
