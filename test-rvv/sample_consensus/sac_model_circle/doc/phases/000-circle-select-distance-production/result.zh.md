# Phase 000 Result: circle2d select/count 生产候选证据

## 当前结论

本阶段为 `SampleConsensusModelCircle2D<PointT>` 的 `selectWithinDistance`
建立了 production RVV（生产 RVV）分流，并重新验证已有 `countWithinDistanceRVV`。
当前证据支持把这个 patch 保留为 PI5 production candidate（生产候选检查点）：
QEMU correctness（正确性）、板卡 correctness、RVV 反汇编归属和 5-run board
production direct（真实生产路径）性能证据都已闭合。

按照 workflow 的 PI5 规则，本阶段结束时它还不是 adopted production behavior（已采用生产行为）。
后续 Phase 040 已按用户“板卡收益即可采纳”的确认完成 production closeout（生产收尾），
当前长期文档为 `doc-rvv/sample_consensus/sac_model_circle-RVV.zh.md`。本文件保留 Phase 000
到达 PI5 checkpoint（生产证据检查点）时的历史状态，恢复当前 topic 时应以后续 Phase 040 result 为准。

## Production Diff 摘要

| 文件 | 变更 |
| --- | --- |
| `sample_consensus/include/pcl/sample_consensus/sac_model_circle.h` | 新增 `selectWithinDistanceStandard` 和 RVV-only `selectWithinDistanceRVV` protected helper。 |
| `sample_consensus/include/pcl/sample_consensus/impl/sac_model_circle.hpp` | public `selectWithinDistance` 在 `__RVV10__`、x/y float field layout、signed 32-bit `pcl::index_t`、u32 byte offset 容量 gate 成立时调用 `selectWithinDistanceRVV`；否则回退 Standard。`countWithinDistance` 也补同样 index/offset gate 后再调用既有 RVV helper。 |
| `test-rvv/sample_consensus/sac_model_circle/src/test_sac_model_circle.cpp` | 覆盖 public/Standard/RVV helper 对拍、乱序 indices、PointXYZI correctness 扩展、cloud-only identity indices 和显式空 indices。 |
| `test-rvv/sample_consensus/sac_model_circle/src/bench_sac_model_circle.cpp` | 公开入口 bench，打印 select/count/getDistances 三项；性能结论只使用板卡 repeated run。 |
| `test-rvv/sample_consensus/sac_model_circle/script/generate_circle_board_evidence_manifest.py` | 解析 Phase 000 select/count production direct repeated board 日志，生成 Evidence Doctor manifest。 |

## 阶段范围

| 维度 | 已验证范围 |
| --- | --- |
| production entry | `selectWithinDistance`、`countWithinDistance` |
| row source | direct indexed `indices_`，bench 使用 shuffled adjacent pairs（相邻乱序对） |
| 点型 / layout | `PointXYZ` correctness + board；`PointXYZI` correctness only |
| 系数 / threshold | `Eigen::VectorXf` circle2d 三系数；`double threshold` 按旧路径转入 float 半径平方边界 |
| RVV gate | x/y registered float fields、signed 32-bit `pcl::index_t`、`rvvMaxU32ByteOffsetElements<PointT>()` 范围内 AoS 点云 |
| 输出语义 | inlier 顺序、`error_sqr_dists_` 精确距离、空 indices 清理语义 |

未验证范围：`getDistancesToModel` 生产 RVV、circle3d、更多 PointXYZ-like 点型的 board、
非 float x/y layout、自定义点型、超大 cloud offset fallback、`Scalar=double` 或其它 row source。

## TDD 和 Correctness 证据

| 证据 | 命令 / 路径 | 结果 |
| --- | --- | --- |
| RED | `make -C test-rvv/sample_consensus/sac_model_circle run_test_rvv TEST_ARGS="--gtest_filter=SampleConsensusModelCircle2D.PublicSelectWithinDistanceMatchesDirectRVVForSupportedLayout"` | 生产 patch 前因缺少 `selectWithinDistanceStandard` / `selectWithinDistanceRVV` helper 编译失败，证明测试能抓住 select 生产缺口。 |
| GREEN | 同上 | 生产 patch 后 1/1 通过。 |
| QEMU correctness | `make -C test-rvv/sample_consensus/sac_model_circle run_test_compare` | Std 4/4、RVV 4/4 通过；日志位于 `test-rvv/sample_consensus/sac_model_circle/log/qemu/run_test_std.log` 和 `run_test_rvv.log`。QEMU 不支撑性能结论。 |
| board correctness | `SSH_AUTH_SOCK=/run/user/1001/gcr/.ssh make -C test-rvv/sample_consensus/sac_model_circle collect_production_repeated_board_evidence` | 5 轮 board smoke 中 RVV gtest 每轮 4/4 通过。 |

## 反汇编和性能证据

反汇编命令：

```bash
make -C test-rvv/sample_consensus/sac_model_circle dump_bench_rvv
```

本轮 manifest wrapper 对 `build/asm/riscv/bench_sac_model_circle_rvv.full.asm`
计数：`selectWithinDistanceRVV` 匹配 23 条 RVV 指令，`countWithinDistanceRVV`
匹配 15 条 RVV 指令。

板卡 repeated 命令：

```bash
SSH_AUTH_SOCK=/run/user/1001/gcr/.ssh \
  make -C test-rvv/sample_consensus/sac_model_circle collect_production_repeated_board_evidence
make -C test-rvv/sample_consensus/sac_model_circle run_production_board_evidence_doctor
```

板卡数据集：`synthetic sac_model_circle direct indexed shell cloud`
（65536 points，shuffled adjacent pairs），每轮 200 iterations，5 warmup iterations，
5-run repeated board，设备记录为 `Milkv-Jupiter`。本轮 remote make 输出存在
clock skew warning（远端文件时间偏差警告），但 checksum 一致、run 完整、decision bucket 未受影响。

| production row | Std mean ms | RVV mean ms | B/A values | median | min/max | `B/A < 1` | decision bucket |
| --- | ---: | ---: | --- | ---: | ---: | ---: | --- |
| `public selectWithinDistance` | 2.774688 | 1.680636 | 1.6758x, 1.6922x, 1.5732x, 1.6702x, 1.6444x | 1.6702x | 1.5732x / 1.6922x | 0/5 | positive |
| `public countWithinDistance` | 0.478622 | 0.340900 | 1.3998x, 1.4216x, 1.4012x, 1.4080x, 1.3898x | 1.4012x | 1.3898x / 1.4216x | 0/5 | positive |

Evidence Doctor（证据体检）：

- manifest：`test-rvv/sample_consensus/sac_model_circle/doc/phases/000-circle-select-distance-production/production-repeated-evidence-manifest.json`
- report：`test-rvv/sample_consensus/sac_model_circle/doc/phases/000-circle-select-distance-production/production-repeated-evidence-doctor.md`
- result：Errors=0，Warnings=0，Suggestions=0

`getDistancesToModel` 仍由 bench 打印作为 companion row（伴随行），但没有进入
Phase 000 production manifest。5-run 观测在 `0.9554x..1.0067x` 间摇摆，
4/5 低于 1；这只支持“当前不把 getDistances 纳入 Phase 000 生产采纳”，不能替代后续
`getDistancesToModel` 独立 ablation（消融）或 probe（探针）。

## EvidenceDecision

| 问题 | 决策 |
| --- | --- |
| select RVV vs scalar | `positive production candidate`：当前 public RVV path 在本阶段 board case 中稳定快于 Std，correctness / asm / doctor 均闭合。 |
| count RVV regression | `positive retained`：既有 count RVV 在同一 board case 中保持稳定收益，且 dispatch gate 已补 index/offset 边界。 |
| getDistances RVV | `continued_by_phase020`：本阶段未改 production；后续 Phase 020 已单独执行 dense distance / sqrt / double store ablation，并把当前 test-only helper 形态关闭为 `rejected with diagnostic evidence`。 |
| clean adoption | Phase 000 结束时为 `blocked_by_PI5_user_confirmation`；后续 Phase 040 已解除该阻塞，并把 select/count gather-style RVV 采纳为当前 production 行为。 |

## Continue / Stop

本阶段当时的合法停止条件是 PI5 用户检查点。后续 Phase 040 已完成用户确认后的 S11 production
closeout，并额外尝试 / 拒绝 identity-index strided load。当前恢复默认入口不应停在本文件的
PI5 pending 状态，而应读取 Phase 040 result、optimization matrix 和 current handoff。
