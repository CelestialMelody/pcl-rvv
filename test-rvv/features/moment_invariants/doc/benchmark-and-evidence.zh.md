# moment_invariants benchmark and evidence

## 本文职责

本文记录 bench（性能测试）case、板卡 summary、Evidence Doctor（证据体检）和 evidence registry（证据登记表）的入口。raw log（原始日志）默认不提交；当前可审查证据以 summary、manifest、doctor 和 registry 为主。

## Bench case 字典

| case-filter | 计时边界 | 证据角色 | 当前结论 |
| --- | --- | --- | --- |
| `mi_accumulation_indexed` | centroid 后 indexed moment accumulation helper | diagnostic（诊断） | Phase 000 板卡 5-run median `1.141x`，weak-positive；历史诊断证据。 |
| `mi_accumulation_full_cloud` | centroid 后 full-cloud moment accumulation helper | diagnostic | QEMU correctness 覆盖，板卡性能未作为当前生产决策主证据。 |
| `mi_public_search_shape` | `pcl::search::KdTree<PointXYZ>` nearestKSearch + test-only helper replacement | production-shaped diagnostic（生产形态诊断） | Phase 010 板卡 5-run median `1.031x`，near-threshold weak-positive；用于解释为什么需要 production-public 复测。 |
| `mi_production_compute_feature` | 真实 `MomentInvariantsEstimation<PointXYZ, MomentInvariants>::compute`，包含 KdTree search、indexed moment accumulation 和 output write | production-public（生产公开入口） | Phase 030 median `1.067x`，0/5 退化，adopted。 |
| `mi_production_compute_feature_pointxyzi` | 真实 `MomentInvariantsEstimation<PointXYZI, MomentInvariants>::compute` | production-public | Phase 040 median `1.071x`，0/5 退化，adopted。 |
| `mi_production_compute_feature_pointxyzrgb` | 真实 `MomentInvariantsEstimation<PointXYZRGB, MomentInvariants>::compute` | production-public | Phase 040 median `1.078x`，0/5 退化，adopted。 |
| `mi_production_compute_feature_pointxyzrgba` | 真实 `MomentInvariantsEstimation<PointXYZRGBA, MomentInvariants>::compute` | production-public | Phase 040 median `1.078x`，0/5 退化，adopted。 |

## CLI 参数

| 参数 | 作用 | 当前使用 |
| --- | --- | --- |
| `--case-filter` | 选择 bench case。 | Phase 030/040 使用 production case-filter。 |
| `--points` | 输入点数；production case 内部限制到 4096。 | Phase 030/040 为 4096。 |
| `--iterations` | 正式计时迭代次数。 | Phase 030/040 为 8。 |
| `--warmup-iterations` | warmup（预热）迭代次数，不计入平均时间。 | Phase 030/040 为 2。 |

## 当前板卡证据

| phase | point type | summary | manifest | doctor | decision bucket |
| --- | --- | --- | --- | --- | --- |
| Phase 030 | `PointXYZ` | `log/board/repeated_phase030_production_compute_feature/summary.md` | `log/board/repeated_phase030_production_compute_feature/evidence_manifest.json` | `log/board/repeated_phase030_production_compute_feature/evidence_doctor.md` | weak-positive，median `1.067x` |
| Phase 040 | `PointXYZI` | `log/board/repeated_phase040_production_pointxyzi_compute_feature/summary.md` | `log/board/repeated_phase040_production_pointxyzi_compute_feature/evidence_manifest.json` | `log/board/repeated_phase040_production_pointxyzi_compute_feature/evidence_doctor.md` | weak-positive，median `1.071x` |
| Phase 040 | `PointXYZRGB` | `log/board/repeated_phase040_production_pointxyzrgb_compute_feature/summary.md` | `log/board/repeated_phase040_production_pointxyzrgb_compute_feature/evidence_manifest.json` | `log/board/repeated_phase040_production_pointxyzrgb_compute_feature/evidence_doctor.md` | weak-positive，median `1.078x` |
| Phase 040 | `PointXYZRGBA` | `log/board/repeated_phase040_production_pointxyzrgba_compute_feature/summary.md` | `log/board/repeated_phase040_production_pointxyzrgba_compute_feature/evidence_manifest.json` | `log/board/repeated_phase040_production_pointxyzrgba_compute_feature/evidence_doctor.md` | weak-positive，median `1.078x` |

## 历史诊断证据

| phase | summary | decision bucket | 证据角色 |
| --- | --- | --- | --- |
| Phase 000 | `log/board/repeated_phase000_moment_accumulation_diagnostic/summary.md` | weak-positive，median `1.141x` | helper-only diagnostic。 |
| Phase 010 | `log/board/repeated_phase010_public_search_shape_diagnostic/summary.md` | weak-positive，median `1.031x` | production-shaped diagnostic；near-threshold suggestion。 |

Phase 000/010 不是最终生产性能依据。它们解释了候选来源和为什么需要 Phase 030/040 接入后复测。

## Registry 可检查路径

| run label | repo-relative evidence paths |
| --- | --- |
| `board-mi-production-compute-feature-phase030` | `test-rvv/features/moment_invariants/log/board/repeated_phase030_production_compute_feature/summary.md`；`test-rvv/features/moment_invariants/log/board/repeated_phase030_production_compute_feature/evidence_manifest.json`；`test-rvv/features/moment_invariants/log/board/repeated_phase030_production_compute_feature/evidence_doctor.md` |
| `board-mi-production-pointxyzi-compute-feature-phase040` | `test-rvv/features/moment_invariants/log/board/repeated_phase040_production_pointxyzi_compute_feature/summary.md`；`test-rvv/features/moment_invariants/log/board/repeated_phase040_production_pointxyzi_compute_feature/evidence_manifest.json`；`test-rvv/features/moment_invariants/log/board/repeated_phase040_production_pointxyzi_compute_feature/evidence_doctor.md` |
| `board-mi-production-pointxyzrgb-compute-feature-phase040` | `test-rvv/features/moment_invariants/log/board/repeated_phase040_production_pointxyzrgb_compute_feature/summary.md`；`test-rvv/features/moment_invariants/log/board/repeated_phase040_production_pointxyzrgb_compute_feature/evidence_manifest.json`；`test-rvv/features/moment_invariants/log/board/repeated_phase040_production_pointxyzrgb_compute_feature/evidence_doctor.md` |
| `board-mi-production-pointxyzrgba-compute-feature-phase040` | `test-rvv/features/moment_invariants/log/board/repeated_phase040_production_pointxyzrgba_compute_feature/summary.md`；`test-rvv/features/moment_invariants/log/board/repeated_phase040_production_pointxyzrgba_compute_feature/evidence_manifest.json`；`test-rvv/features/moment_invariants/log/board/repeated_phase040_production_pointxyzrgba_compute_feature/evidence_doctor.md` |

## Evidence Doctor 结果

Phase 030/040 production-public summaries 均为 0 Error / 0 Warning / 2 Suggestion。Suggestion 内容相同：缺 taskset、governor、freq、temperature 和 binary hash。由于每组 5-run 全部正向、decision bucket 稳定，这些 suggestion 不阻塞 adopted production behavior；若后续出现长尾或方向反转，应先补 metadata 后重跑。

## ASM 口径

| 命令 | 证明 |
| --- | --- |
| `make check_production_rvv_asm` | `PointXYZ` production helper / public symbol 中有 RVV indexed load 和 reduction。 |
| `make check_production_pointxyzi_rvv_asm` | `PointXYZI` typed production path 命中 RVV。 |
| `make check_production_pointxyzrgb_rvv_asm` | `PointXYZRGB` typed production path 命中 RVV。 |
| `make check_production_pointxyzrgba_rvv_asm` | `PointXYZRGBA` typed production path 命中 RVV。 |

反汇编只能证明路径归属，性能结论仍以板卡 repeated summary 为准。

## 提交边界

summary、manifest、doctor 和 `log/evidence_registry.json` 被 topic-local 文档和 `doc-rvv` 引用时可进入 review 候选；raw `run-*` 日志、build 输出和远端目录不默认提交。
