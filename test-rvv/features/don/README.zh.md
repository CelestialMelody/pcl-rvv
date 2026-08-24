# DON RVV topic closeout

## 当前状态

`features/include/pcl/features/impl/don.hpp` 的 DON（Difference of Normals，法线差分）topic 已暂停为 no-production（不接入生产）。当前没有保持 production（生产）语义且值得继续推进的 RVV production 方向。

当前保留的 production 源码变化只有 `DifferenceOfNormalsEstimation::initCompute()` 调用 `PCLBase<PointInT>::initCompute()` 的独立正确性修复；phase 030 的 RVV production helper / dispatch 已按用户确认回滚。

## 先读路径

| 读者问题 | 路径 |
| --- | --- |
| 当前结论和标量路径 | `doc/don-evaluation.zh.md` |
| 阶段索引 | `doc/phases/README.zh.md` |
| 最终停止条件 | `doc/phases/050-production-detail-ablation/result.zh.md` |
| 优化矩阵 | `doc/phases/optimization-matrix.zh.md` |
| 后续恢复条件 | `doc/optimization-roadmap.zh.md` |

## 可复现命令

| 目的 | 命令 | 当前含义 |
| --- | --- | --- |
| QEMU correctness（QEMU 正确性） | `make -C test-rvv/features/don run_test_compare` | Std/RVV build 各 5 个 gtest 通过 |
| QEMU detail-ablation smoke（日志形状 smoke） | `make -C test-rvv/features/don run_bench_std BENCH_ARGS='--points 4096 --iterations 1 --warmup-iterations 1 --case-filter don_ablate_all'`；随后运行 `run_bench_rvv` 和 `analyze_bench_compare` | 只证明 case 可运行、checksum 对齐和日志可解析，不证明性能 |
| RVV asm attribution（反汇编归属） | `make -C test-rvv/features/don dump_bench_rvv` | test-only helper binary 中有 RVV 指令 |
| board repeated（板卡重复性能） | `make -C test-rvv/features/don run_board_don_ablation_repeated` | phase 050 的真实性能证据 |
| evidence registry（证据登记） | `make -C test-rvv/features/don evidence_status` | 当前登记 fresh |

## Doc Suite Role Inventory

| role | decision | path / section | evidence |
| --- | --- | --- | --- |
| topic navigation | standalone | `README.zh.md` | 本文件 |
| testing overview | merged | `README.zh.md#可复现命令` | test / bench / board / asm target 已列出 |
| correctness tests | merged | `doc/don-evaluation.zh.md#诊断证据链` | gtest 和 public entry correctness 已说明 |
| benchmark and evidence | merged | `doc/don-evaluation.zh.md#Production direct 证据链`，`doc/phases/050-production-detail-ablation/result.zh.md#证据链` | diagnostic、production-public、detail-ablation 分层 |
| optimization evidence | standalone | `doc/phases/optimization-matrix.zh.md` | 候选、证据、decision、next action 已矩阵化 |
| optimization roadmap | standalone | `doc/optimization-roadmap.zh.md` | 恢复条件和拒绝路线已列出 |
| test support code map | merged | `doc/don-evaluation.zh.md#Traceability Map（可追踪性地图）` | helper、bench、script、production entry 已映射 |
| phase index | standalone | `doc/phases/README.zh.md` | phase 000-050 状态已列出 |
| evaluation | standalone | `doc/don-evaluation.zh.md` | EvidenceDecision 和 production 接入判断已更新 |
| production topic doc | not_applicable with evidence | none | 当前为 rollback/no-production，无 adopted production RVV behavior |

## 提交边界

本 topic commit 使用 `topic-only` 偏好：提交 DON topic 源码、测试资产、bench、脚本、topic-local 文档、features 队列表和 `don.hpp` 的独立正确性修复；不提交 `build/`、`log/`、raw board logs、QEMU logs、`__pycache__/`、本机配置或其它 topic 改动。
