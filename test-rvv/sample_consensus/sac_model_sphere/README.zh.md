# sac_model_sphere RVV topic 导航

## 当前结论

当前 production（生产源码）在 `countWithinDistance` 和 `selectWithinDistance` 都有 RVV path（RVV 路径）。
Phase 045/046 已把 `selectWithinDistance` 的 `vcompress` 候选采纳为真实公开入口的当前生产实现，接入后
5-run board repeated（板卡重复性能测试）median speedup 为 `2.0989x`，min/max 为
`2.0867x / 2.1404x`。既有 `countWithinDistance` production RVV 回归仍稳定正向，
median 为 `3.5154x`。Phase 050 又补充了 `PointXYZI` 接入后板卡证据：public
`selectWithinDistance<PointXYZI>` median 为 `1.5901x`，min/max 为 `1.4672x / 1.6317x`。
Phase 060 补齐 RGB/RGBA 点型证据：`PointXYZRGB` public select median 为 `1.6225x`，
5/5 run 正向；`PointXYZRGBA` public select median 为 `1.5528x`，但 1/5 run 为
`0.9184x`，带退化和长尾 warning。

`selectWithinDistance` 的 production dispatch（生产分流）只覆盖当前批准范围：direct indexed `indices_`、
`RVVXYZFloatLayout<PointT>`、32-bit byte offset gate（32 位字节偏移准入条件）以及 `PointXYZ` /
`PointXYZI` / `PointXYZRGB` / `PointXYZRGBA` 板卡性能。自定义 registered xyz 点型仍未验证。

`getDistancesToModel` 当前保持标量。测试专用 `RVV squared-distance + scalar sqrt/store` 候选在
Phase 020 production repeated evidence（生产重复证据）中 median speedup 为 `0.7775x`，
当前实现族 rejected（拒绝），不接入 production。

## 先读哪份文档

| 目的 | 入口 |
| --- | --- |
| 恢复当前阶段 | `doc/phases/README.zh.md` |
| 看函数入口和 EvidenceDecision（证据决策） | `doc/sac_model_sphere-evaluation.zh.md` |
| 看测试入口分类 | `doc/testing-overview.zh.md` |
| 看 gtest 覆盖 | `doc/correctness-tests.zh.md` |
| 看 bench、board、manifest 和 Evidence Doctor（证据体检） | `doc/benchmark-and-evidence.zh.md` |
| 看候选取舍 | `doc/optimization-evidence.zh.md` |
| 看测试支撑代码定位 | `doc/test-support-code-map.zh.md` |
| 看跨阶段候选搜索空间 | `doc/optimization-roadmap.zh.md` |
| 看阶段矩阵 | `doc/phases/optimization-matrix.zh.md` |

## 常用命令

```bash
make -C test-rvv/sample_consensus/sac_model_sphere run_test_compare
make -C test-rvv/sample_consensus/sac_model_sphere dump_bench_rvv
SSH_AUTH_SOCK=<ssh-agent-socket> make -C test-rvv/sample_consensus/sac_model_sphere check_board_ssh
SSH_AUTH_SOCK=<ssh-agent-socket> make -C test-rvv/sample_consensus/sac_model_sphere board_smoke
make -C test-rvv/sample_consensus/sac_model_sphere generate_board_evidence_manifest
python3 test-rvv/script/evidence_doctor.py --manifest test-rvv/sample_consensus/sac_model_sphere/doc/phases/000-sphere-select-distance-diagnostic/repeated-evidence-manifest.json --output test-rvv/sample_consensus/sac_model_sphere/doc/phases/000-sphere-select-distance-diagnostic/repeated-evidence-doctor.md --fail-on never
make -C test-rvv/sample_consensus/sac_model_sphere record_repeated_board_evidence_state repeated_evidence_status
SSH_AUTH_SOCK=<ssh-agent-socket> make -C test-rvv/sample_consensus/sac_model_sphere collect_production_repeated_board_evidence
make -C test-rvv/sample_consensus/sac_model_sphere record_production_board_evidence_state production_evidence_status
SSH_AUTH_SOCK=<ssh-agent-socket> make -C test-rvv/sample_consensus/sac_model_sphere collect_production_vcompress_repeated_board_evidence
make -C test-rvv/sample_consensus/sac_model_sphere record_production_vcompress_board_evidence_state production_vcompress_evidence_status
make -C test-rvv/sample_consensus/sac_model_sphere run_bench_rvv BENCH_ARGS='128 1 PointXYZI'
SSH_AUTH_SOCK=<ssh-agent-socket> make -C test-rvv/sample_consensus/sac_model_sphere collect_point_type_repeated_board_evidence
make -C test-rvv/sample_consensus/sac_model_sphere record_point_type_board_evidence_state point_type_evidence_status
make -C test-rvv/sample_consensus/sac_model_sphere run_bench_rvv BENCH_ARGS='128 1 PointXYZRGB'
make -C test-rvv/sample_consensus/sac_model_sphere run_bench_rvv BENCH_ARGS='128 1 PointXYZRGBA'
SSH_AUTH_SOCK=<ssh-agent-socket> make -C test-rvv/sample_consensus/sac_model_sphere collect_rgb_point_type_repeated_board_evidence
SSH_AUTH_SOCK=<ssh-agent-socket> make -C test-rvv/sample_consensus/sac_model_sphere collect_rgba_point_type_repeated_board_evidence
make -C test-rvv/sample_consensus/sac_model_sphere record_rgb_point_type_board_evidence_state rgb_point_type_evidence_status
make -C test-rvv/sample_consensus/sac_model_sphere record_rgba_point_type_board_evidence_state rgba_point_type_evidence_status
```

QEMU 只用于 correctness（正确性）和日志形状；性能结论必须来自 board（板卡）或目标硬件。

## 当前可提交证据

| 类型 | 路径 | 边界 |
| --- | --- | --- |
| phase result | `doc/phases/000-sphere-select-distance-diagnostic/result.zh.md` | 当前诊断结论主归属。 |
| repeated manifest | `doc/phases/000-sphere-select-distance-diagnostic/repeated-evidence-manifest.json` | summary evidence，可复核 5-run board 数据。 |
| repeated doctor | `doc/phases/000-sphere-select-distance-diagnostic/repeated-evidence-doctor.md` | summary evidence，列出 Error / Suggestion 和降级动作。 |
| repeated doctor JSON | `doc/phases/000-sphere-select-distance-diagnostic/repeated-evidence-doctor.json` | summary evidence 的机器可读检查结果。 |
| production phase result | `doc/phases/020-select-production-integration-plan/result.zh.md` | `selectWithinDistance` 生产接入后的 PI2-PI5 结果主归属。 |
| production manifest | `doc/phases/020-select-production-integration-plan/production-repeated-evidence-manifest.json` | summary evidence，可复核接入后 5-run board 数据。 |
| production doctor | `doc/phases/020-select-production-integration-plan/production-repeated-evidence-doctor.md` | summary evidence，`selectWithinDistance` 行无 Error / Warning；`getDistancesToModel` Error 已降级。 |
| Phase 045 vcompress result | `doc/phases/045-select-vcompress-production-integration/result.zh.md` | `vcompress` production patch 的 PI2-PI5 结果；PI5 确认由 Phase 046 解除。 |
| Phase 046 closeout result | `doc/phases/046-vcompress-production-closeout/result.zh.md` | 用户确认收益即可采纳后的 adopted closeout。 |
| Phase 045 vcompress manifest | `doc/phases/045-select-vcompress-production-integration/production-vcompress-repeated-evidence-manifest.json` | summary evidence，可复核 5-run public select `2.0989x`。 |
| Phase 045 vcompress doctor | `doc/phases/045-select-vcompress-production-integration/production-vcompress-repeated-evidence-doctor.md` | summary evidence，`selectWithinDistance` 行无 Error / Warning；`getDistancesToModel` Error 已降级。 |
| Phase 050 PointXYZI manifest | `doc/phases/050-point-type-expansion/point-type-repeated-evidence-manifest.json` | summary evidence，可复核 5-run `PointXYZI` public select `1.5901x`。 |
| Phase 050 PointXYZI doctor | `doc/phases/050-point-type-expansion/point-type-repeated-evidence-doctor.md` | summary evidence，`PointXYZI` select 行无 Error / Warning；`getDistancesToModel` Error 已降级。 |
| Phase 050 PointXYZI doctor JSON | `doc/phases/050-point-type-expansion/point-type-repeated-evidence-doctor.json` | summary evidence 的机器可读检查结果。 |
| Phase 060 RGB/RGBA result | `doc/phases/060-point-type-rgb-rgba-expansion/result.zh.md` | 记录 RGB/RGBA 点型扩展的接入后板卡结论和稳定性 warning。 |
| Phase 060 RGB/RGBA manifests / doctors | `doc/phases/060-point-type-rgb-rgba-expansion/*point-type-repeated-evidence-*` | summary evidence，可复核 RGB median `1.6225x`、RGBA median `1.5528x`。 |
| evidence registry | `log/evidence_registry.json` | freshness metadata，用 `repeated_evidence_status` 检查当前登记状态。 |
| evaluation / role docs | `doc/*.zh.md` | topic-local 文档，review 后可提交。 |
| production topic doc | `doc-rvv/sample_consensus/sac_model_sphere-RVV.zh.md` | production 长期主题文档，记录已采纳生产行为和证据链。 |

## 默认不提交

`build/` 二进制、`build/asm/` 生成反汇编、`log/board/` raw logs、远端 `/root/...` 路径、本机 `config.mk` 和私有 board 地址默认不提交。若用户明确要求提交 evidence logs（证据日志），先脱敏并单独审查。

## production_topic_doc 适用性

`doc-rvv/sample_consensus/sac_model_sphere-RVV.zh.md` 当前 applicable（适用）：`selectWithinDistance`
已有 Phase 045/046 adopted production behavior（已采用生产行为），长期文档记录当前 `vcompress`
实现、Phase 050 `PointXYZI` 点型扩展证据，以及 Phase 060 RGB/RGBA 点型扩展证据。
`getDistancesToModel` 没有 adopted production behavior，只在 topic-local evaluation（函数级评估）
和 phase result 中保留拒绝理由。

## 当前测试支撑结构

Phase 055 已把测试支撑迁移到 topic-local `include` / `include/impl` 结构：
`src/test_sac_model_sphere.cpp` 只保留 gtest case，`src/bench_sac_model_sphere.cpp`
只保留 CLI 解析；共享 access wrapper、fixture、assertion 和 bench harness 分别位于
`include/impl/sac_model_sphere_access.hpp`、`include/test_sac_model_sphere.h` 和
`include/bench_sac_model_sphere.h`。

这次迁移回应了与
`test-rvv/registration/transformation_estimation_point_to_plane_lls/include` 的结构差异：
`.agents/config/defaults.yaml` 中的
`test_support.aggregator_directory=include`、`test_support.internal_directory=include/impl`
以及 phase-loop 规则都要求把这类结构缺口作为可恢复 phase 处理。Phase 060 已继续补齐
`PointXYZRGB` / `PointXYZRGBA` 的接入后板卡证据。
