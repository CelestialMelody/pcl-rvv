# keypoints/harris_2d RVV Topic

## 当前结论

当前 topic 已完成 Phase 020 production direct（真实生产入口直连）闭环，并按本轮 prompt override 采纳生产补丁。`HarrisKeypoint2D<PointXYZI, PointXYZI>` 在 `__RVV10__` 且输入 `is_dense` 时由 `responseHarris/Noble/Lowe/Tomasi()` 分流到 `responseRVV()`；非 RVV 构建或非 dense 输入保持标量路径。

本轮还修复了 `computeSecondMomentMatrix()` 中 `width` / `height` 函数内 `static` 导致的同进程多尺寸输入状态泄漏。该修复影响标量和 RVV 两侧 public entry（公开入口），因此旧 phase020 板卡数据已降级为 historical evidence（历史证据）；当前采用 `log/board/repeated_phase020_direct_intensity_stride_store_public_entry/` 中重新采集并登记的结果。

## 阅读路径

| 目的 | 路径 |
| --- | --- |
| 正式生产长期文档 | `doc-rvv/keypoints/harris_2d-RVV.zh.md` |
| 函数级评估和生产接入判断 | `test-rvv/keypoints/harris_2d/doc/harris_2d-evaluation.zh.md` |
| Phase 020 结果 | `test-rvv/keypoints/harris_2d/doc/phases/020-direct-intensity-stride-store/result.zh.md` |
| 阶段索引 | `test-rvv/keypoints/harris_2d/doc/phases/README.zh.md` |
| 优化矩阵 | `test-rvv/keypoints/harris_2d/doc/phases/optimization-matrix.zh.md` |
| 优化路线图 | `test-rvv/keypoints/harris_2d/doc/optimization-roadmap.zh.md` |
| 测试入口总览 | `test-rvv/keypoints/harris_2d/doc/testing-overview.zh.md` |
| 正确性测试说明 | `test-rvv/keypoints/harris_2d/doc/correctness-tests.zh.md` |
| bench 和证据说明 | `test-rvv/keypoints/harris_2d/doc/benchmark-and-evidence.zh.md` |
| 优化证据索引 | `test-rvv/keypoints/harris_2d/doc/optimization-evidence.zh.md` |
| 测试支撑代码地图 | `test-rvv/keypoints/harris_2d/doc/test-support-code-map.zh.md` |

## 常用命令

| 命令 | 用途 |
| --- | --- |
| `make -C test-rvv/keypoints/harris_2d run_test_compare` | QEMU correctness（正确性）对拍，Std/RVV 两侧都跑。 |
| `make -C test-rvv/keypoints/harris_2d check_harris_2d_rvv_asm` | 生成并检查 RVV 指令反汇编。 |
| `make -C test-rvv/keypoints/harris_2d board_repeated record_evidence_state_repeated REPEATED_BOARD_TAG=phase020_direct_intensity_stride_store_public_entry HARRIS2D_REPEATED_BENCH_ARGS="--case-filter all --iterations 20 --warmup-iterations 3 --public-entry"` | 板卡 5-run production direct 证据、manifest（证据清单）、Evidence Doctor（证据体检）和 registry（登记表）。 |
| `make -C test-rvv/keypoints/harris_2d check_evidence_freshness` | 检查当前 summary / manifest / doctor 是否登记且被文档引用。 |

## 证据白名单

当前可提交摘要证据是 `test-rvv/keypoints/harris_2d/log/board/repeated_phase020_direct_intensity_stride_store_public_entry/summary.md`、`evidence_manifest.json`、`evidence_doctor.md` 和 `evidence_doctor.json`。raw run logs、build output、远端路径和本机 `config.mk` 默认不提交。

## 文档套件状态

| role | status |
| --- | --- |
| topic_navigation | `standalone:test-rvv/keypoints/harris_2d/README.zh.md` |
| testing_overview | `standalone:test-rvv/keypoints/harris_2d/doc/testing-overview.zh.md` |
| correctness_tests | `standalone:test-rvv/keypoints/harris_2d/doc/correctness-tests.zh.md` |
| benchmark_and_evidence | `standalone:test-rvv/keypoints/harris_2d/doc/benchmark-and-evidence.zh.md` |
| optimization_evidence | `standalone:test-rvv/keypoints/harris_2d/doc/optimization-evidence.zh.md` |
| optimization_roadmap | `standalone:test-rvv/keypoints/harris_2d/doc/optimization-roadmap.zh.md` |
| test_support_code_map | `standalone:test-rvv/keypoints/harris_2d/doc/test-support-code-map.zh.md` |
| phase_index | `standalone:test-rvv/keypoints/harris_2d/doc/phases/README.zh.md` |
| evaluation | `standalone:test-rvv/keypoints/harris_2d/doc/harris_2d-evaluation.zh.md` |
| production_topic_doc | `standalone:doc-rvv/keypoints/harris_2d-RVV.zh.md` |
