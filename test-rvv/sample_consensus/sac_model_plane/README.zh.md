# sac_model_plane RVV topic

## 当前结论

`SampleConsensusModelPlane<PointT>` 的 `selectWithinDistance`、`countWithinDistance` 和
`getDistancesToModel` 已保留 production RVV（生产 RVV）路径。基础采纳范围是
`__RVV10__` 构建、registered single-float xyz AoS（已注册单精度 xyz 结构数组）点型、
32-bit byte offset（32 位字节偏移）可表达的点云规模，以及 direct indexed `indices_`
行来源。

Phase 010 又采纳了一个更窄的 identity-index fast path（恒等索引快速路径）：只有
`selectWithinDistanceRVV` 和 `countWithinDistanceRVV` 在 `indices[i] == i` 的 VL chunk
（可变向量长度分块）上使用 strided load（跨步加载）。`getDistancesToModelRVV` 已尝试同类
快速路径，但因 mixed gate（混合加载分流）没有稳定改善且 shuffled case（乱序索引用例）曾回落，
当前仍保持 indexed gather（按索引离散加载）。

长期生产说明见 `doc-rvv/sample_consensus/sac_model_plane-RVV.zh.md`。

## 阅读顺序

| 文档 | 职责 |
| --- | --- |
| `doc/sac_model_plane-evaluation.zh.md` | 函数级评估、EvidenceDecision（证据决策）、Traceability Map（可追踪性地图）。 |
| `doc/testing-overview.zh.md` | 测试入口分类、target 粒度和证据边界。 |
| `doc/correctness-tests.zh.md` | 每个 gtest 的输入、断言和覆盖范围。 |
| `doc/benchmark-and-evidence.zh.md` | bench（性能测试）、board（板卡）、manifest（证据清单）和 Evidence Doctor（证据体检）。 |
| `doc/optimization-evidence.zh.md` | 已采用、已拒绝、暂缓和后续优化方式的证据索引。 |
| `doc/test-support-code-map.zh.md` | 测试支撑代码、脚本、日志和 production helper 定位。 |
| `doc/optimization-roadmap.zh.md` | 跨 phase（阶段）的候选队列和默认恢复动作。 |
| `doc/phases/README.zh.md` | 阶段计划 / 结果索引。 |

## 常用命令

```bash
make -C test-rvv/sample_consensus/sac_model_plane run_test_compare
make -B -C test-rvv/sample_consensus/sac_model_plane dump_bench_rvv
SSH_AUTH_SOCK=<agent-socket> make -C test-rvv/sample_consensus/sac_model_plane board_smoke REMOTE_USER=<board-user> REMOTE_IP=<board-ip> BENCH_ARGS='65536 200 identity'
SSH_AUTH_SOCK=<agent-socket> make -C test-rvv/sample_consensus/sac_model_plane board_smoke REMOTE_USER=<board-user> REMOTE_IP=<board-ip> BENCH_ARGS='65536 200 shuffled'
make -C test-rvv/sample_consensus/sac_model_plane generate_phase_010_identity_evidence_manifest
make -C test-rvv/sample_consensus/sac_model_plane generate_phase_010_shuffled_evidence_manifest
```

QEMU（仿真器）只用于 correctness（正确性）和日志形状；性能结论只引用 repeated board
（重复板卡测试）或目标硬件结果。

## 证据提交边界

可提交的摘要证据是：

- `doc/phases/000-base-plane-select-distance-production/evidence-doctor.md`
- `doc/phases/010-identity-index-strided-load/evidence-doctor.md`
- `log/board/evidence_manifest.json`
- `log/board/phase-010/identity-stride-select-count/repeated/evidence_manifest.json`
- `log/board/phase-010/shuffled-stride-select-count/repeated/evidence_manifest.json`

`log/board/**/run-*`、`log/board/*.log`、`log/qemu/*.log` 和 `build/` 默认是本地输出；
只有用户明确要求提交日志时，才按 sanitize（脱敏）策略处理。

## 默认恢复动作

当前性能优化的默认恢复动作不是继续扩展 identity fast path。Phase 010 已证明 select/count
值得窄采纳，`getDistancesToModel` 不值得采纳。Phase 020 已补 `PointXYZI`、`PointXYZRGB`、
`PointXYZRGBA` 和 `PointXYZINormal` 的代表点型 correctness（正确性）证据；Phase 025 又补了
显式空 `indices_` 回归测试。当前 QEMU 和板卡 RVV gtest 均为 7/7 通过。

当前 topic 内没有新的未阻塞 RVV performance candidate（性能候选）。剩余可做的是
evidence registry / 环境 metadata（元数据）硬化；它服务提交和长期归档，不改变性能采纳结论。
