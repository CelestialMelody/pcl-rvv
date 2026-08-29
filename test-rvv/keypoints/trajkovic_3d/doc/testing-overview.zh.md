# Trajkovic 3D 测试总览

## 当前结论

本 topic 的测试覆盖 TrajkovicKeypoint3D `FOUR_CORNERS` / `EIGHT_CORNERS` response map（四邻域 / 八邻域响应图）的 diagnostic helper（诊断辅助函数）和 production public dispatch（生产公开入口分流）。当前 production adopted scope（已采纳生产范围）是 dense `PointXYZ + Normal`、3x3、预计算法线路径。

## Target 粒度审计

| target 类别 | 当前入口 | 作用 | 状态 |
| --- | --- | --- | --- |
| correctness aggregate（正确性汇总入口） | `make run_test_compare` | 分别运行 Std/RVV gtest。 | adopted |
| QEMU smoke（QEMU 小型验证） | `make run_qemu_smoke` | 作为 `run_test_compare` 别名，只证明 correctness 和日志形状。 | adopted |
| asm gate（反汇编验收） | `make check_trajkovic_3d_rvv_asm` | 检查 `vlse32.v`、`vfmul.vv`、`vfsqrt.v`。 | adopted |
| diagnostic bench | `make run_bench_compare BENCH_ARGS="--case-filter all ..."` | response-only helper benchmark。 | adopted |
| production public bench | `BENCH_ARGS="--mode public --case-filter all ..."` | 真实 `Detector::compute()` benchmark。 | adopted |
| board repeated diagnostic | `make board_repeated record_evidence_state_repeated` | 生成 Phase 000 repeated summary / manifest / doctor / registry。 | adopted |
| board repeated production | `make board_repeated_production record_evidence_state_production` | 生成 Phase 020 production summary / manifest / doctor / registry。 | adopted |
| doctor / registry | `make evidence_doctor_* record_evidence_state_* check_evidence_freshness` | 检查证据契约和文档引用。 | adopted |
| historical probe guarded aliases | 无历史回滚探针。 | 当前 topic 没有旧 production probe。 | not_applicable with evidence |

## 覆盖矩阵

| 覆盖项 | 测试 / 证据 | 证明 | 不证明 |
| --- | --- | --- | --- |
| response formula | `Trajkovic3DFourCornersResponse.*` | Std/RVV helper 对拍。 | public NMS 性能。 |
| invalid neighbor | `TreatsInvalidNeighborNormalsAsNullNormals` | 非法邻域 normal 按 null normal 处理。 | production dense 主路径性能。 |
| public output | `PublicComputeFourCornersMatchesResponseReference`、`PublicComputeInvalidAndTailMatchesResponseReference`、`PublicComputeEightCornersInvalidAndTailMatchesResponseReference` | `compute()` 输出 index 和 intensity 与 reference NMS 对齐。 | public NMS 性能和更宽点型。 |
| fallback / tiny | `PublicComputeTinyCloudFallsBackWithoutResponses` | 小输入不会误写响应。 | 大规模性能。 |
| production board | Phase 020 summary | 真实 public compute 主路径收益。 | normal estimation、泛型点型和 non-dense RVV。 |

## 证据提交边界

`log/**` 默认被 `test-rvv/.gitignore` 排除。summary / manifest / doctor 只有在文档引用且 reviewer 需要复核时才进入提交候选；raw run logs 默认不提交。本 topic 的长期性能结论引用 `log/board/repeated_phase020_eight_corners_production_public/summary.md`、`evidence_manifest.json` 和 `evidence_doctor.md`。
