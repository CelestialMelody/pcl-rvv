# recognition/color_modality RVV Topic

## 当前结论

`ColorModality<PointXYZRGB>::processInputData()` 已采用窄范围 production RVV（生产 RVV 路径）。当前 RVV 路径接管 RGB extrema quantize（RGB 极值颜色量化）和 3x3 dominant filter（主桶滤波），再调用既有 `QuantizedMap::spreadQuantizedMap()`。`extractFeatures()` 和 `computeDistanceMap()` 保持标量。

当前采纳依据是 Phase 040 production direct（真实生产路径）板卡重复测试：`production_process_320x240` median `2.490x`，`production_process_641x481_tail` median `2.410x`，两组 `B/A < 1` 都是 `0/5`，checksum 稳定。用户已授权“板卡有收益即可采纳”，因此本 topic 当前生产补丁按 adopted production behavior（已采用生产行为）记录。

## 阅读路径

| 目的 | 路径 |
| --- | --- |
| 函数级评估和生产接入判断 | `test-rvv/recognition/color_modality/doc/color_modality-evaluation.zh.md` |
| 阶段索引 | `test-rvv/recognition/color_modality/doc/phases/README.zh.md` |
| 当前 Phase 040 结果 | `test-rvv/recognition/color_modality/doc/phases/040-rgb-extrema-quantize-production-rvv/result.zh.md` |
| 优化矩阵 | `test-rvv/recognition/color_modality/doc/phases/optimization-matrix.zh.md` |
| 优化路线图 | `test-rvv/recognition/color_modality/doc/optimization-roadmap.zh.md` |
| 测试入口总览 | `test-rvv/recognition/color_modality/doc/testing-overview.zh.md` |
| 正确性测试说明 | `test-rvv/recognition/color_modality/doc/correctness-tests.zh.md` |
| bench 和证据说明 | `test-rvv/recognition/color_modality/doc/benchmark-and-evidence.zh.md` |
| 优化证据索引 | `test-rvv/recognition/color_modality/doc/optimization-evidence.zh.md` |
| 测试支撑代码地图 | `test-rvv/recognition/color_modality/doc/test-support-code-map.zh.md` |
| 长期生产说明 | `doc-rvv/recognition/color_modality-RVV.zh.md` |

## 常用命令

| 命令 | 用途 |
| --- | --- |
| `make -C test-rvv/recognition/color_modality run_test_compare` | QEMU correctness（正确性）对拍，Std/RVV 两侧都跑。 |
| `make -C test-rvv/recognition/color_modality run_bench_rvv BENCH_ARGS="--case-filter production_process_320x240 --iterations 1 --warmup-iterations 1"` | QEMU bench smoke（小型可运行性验证），不作为性能证据。 |
| `make -C test-rvv/recognition/color_modality check_cm_rvv_asm` | 生成并检查 RVV 指令反汇编。 |
| `SSH_AUTH_SOCK=/run/user/1001/keyring/ssh make -C test-rvv/recognition/color_modality board_repeated record_evidence_state_repeated` | 板卡 5-run production direct 证据、manifest、Evidence Doctor（证据体检）和 registry。 |
| `make -C test-rvv/recognition/color_modality check_evidence_freshness` | 检查当前 summary / manifest / doctor 是否登记且被文档引用。 |

## 证据白名单

当前可提交摘要证据是 `test-rvv/recognition/color_modality/log/board/repeated_phase040_quantize_filter_rvv/summary.md`、`evidence_manifest.json`、`evidence_doctor.md` 和 `evidence_doctor.json`。raw run logs、build output、远端路径和本机 `config.mk` 默认不提交。

## 文档套件状态

| role | status |
| --- | --- |
| topic_navigation | `standalone:test-rvv/recognition/color_modality/README.zh.md` |
| testing_overview | `standalone:test-rvv/recognition/color_modality/doc/testing-overview.zh.md` |
| correctness_tests | `standalone:test-rvv/recognition/color_modality/doc/correctness-tests.zh.md` |
| benchmark_and_evidence | `standalone:test-rvv/recognition/color_modality/doc/benchmark-and-evidence.zh.md` |
| optimization_evidence | `standalone:test-rvv/recognition/color_modality/doc/optimization-evidence.zh.md` |
| optimization_roadmap | `standalone:test-rvv/recognition/color_modality/doc/optimization-roadmap.zh.md` |
| test_support_code_map | `standalone:test-rvv/recognition/color_modality/doc/test-support-code-map.zh.md` |
| phase_index | `standalone:test-rvv/recognition/color_modality/doc/phases/README.zh.md` |
| evaluation_production | `standalone:test-rvv/recognition/color_modality/doc/color_modality-evaluation.zh.md` |
| production_topic_doc | `standalone:doc-rvv/recognition/color_modality-RVV.zh.md` |
