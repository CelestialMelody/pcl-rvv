# sac_model_sphere phase 索引

本文是 `sample_consensus/sac_model_sphere` topic 的阶段恢复入口。阶段文档保存计划、结果、Evidence Doctor（证据体检）和 optimization matrix（优化矩阵）；Phase 020 后长期 `doc-rvv` 主题文档已经适用于 `selectWithinDistance` 当前 production scope。

| phase | 状态 | 入口 | 结果 | 默认恢复动作 |
| --- | --- | --- | --- | --- |
| `000-sphere-select-distance-diagnostic` | `complete` | `doc/phases/000-sphere-select-distance-diagnostic/plan.zh.md` | `doc/phases/000-sphere-select-distance-diagnostic/result.zh.md` | 已关闭测试专用 select/getDistances 诊断证据；select 为 `partial-production-candidate`，当前 getDistances 候选被拒绝。 |
| `010-structure-parity-doc-suite` | `complete` | `doc/phases/010-structure-parity-doc-suite/plan.zh.md` | `doc/phases/010-structure-parity-doc-suite/result.zh.md` | 已补 README、testing overview、correctness tests、benchmark/evidence、optimization evidence 和 test-support code map。 |
| `015-evidence-registry-target-alias` | `complete` | `doc/phases/015-evidence-registry-target-alias/plan.zh.md` | `doc/phases/015-evidence-registry-target-alias/result.zh.md` | 已补 repeated Evidence Doctor / registry target，`repeated_evidence_status` 为 fresh。 |
| `020-select-production-integration-plan` | `complete` | `doc/phases/020-select-production-integration-plan/plan.zh.md` | `doc/phases/020-select-production-integration-plan/result.zh.md` | PI2-PI5 已完成；`selectWithinDistance` production direct 5-run median `1.5020x`，当前 scope adopted。 |
| `040-select-vcompress-ablation` | `complete` | `doc/phases/040-select-vcompress-ablation/plan.zh.md` | `doc/phases/040-select-vcompress-ablation/result.zh.md` | 测试专用 `vcompress` 候选相对 Phase 020 production helper median `1.3020x`，进入有界生产探针。 |
| `045-select-vcompress-production-integration` | `complete` | `doc/phases/045-select-vcompress-production-integration/plan.zh.md` | `doc/phases/045-select-vcompress-production-integration/result.zh.md` | production `vcompress` patch 当前重跑 public select median `2.0989x`；PI5 用户确认已由 Phase 046 解除。 |
| `046-vcompress-production-closeout` | `complete` | `doc/phases/046-vcompress-production-closeout/plan.zh.md` | `doc/phases/046-vcompress-production-closeout/result.zh.md` | 用户确认收益即可采纳后，`selectWithinDistance` `vcompress` 刷新为 adopted/current production behavior。 |
| `050-point-type-expansion` | `complete` | `doc/phases/050-point-type-expansion/plan.zh.md` | `doc/phases/050-point-type-expansion/result.zh.md` | `PointXYZI` production public select 5-run median `1.5901x`，支持保持当前 generic xyz layout gate。 |
| `055-test-support-include-layout` | `complete` | `doc/phases/055-test-support-include-layout/plan.zh.md` | `doc/phases/055-test-support-include-layout/result.zh.md` | 已按 `include` / `include/impl` 迁移测试支撑，保持 gtest、bench 输出和 evidence target 合同不变。 |
| `060-point-type-rgb-rgba-expansion` | `complete` | `doc/phases/060-point-type-rgb-rgba-expansion/plan.zh.md` | `doc/phases/060-point-type-rgb-rgba-expansion/result.zh.md` | `PointXYZRGB` public select median `1.6225x` 且 5/5 run 正向；`PointXYZRGBA` median `1.5528x` 但 1/5 run 退化。 |

## 当前证据边界

- `countWithinDistance` 已有 production RVV path（生产 RVV 路径）。
- `selectWithinDistance` Phase 045/046 已采纳 `vcompress` production patch，覆盖 direct indexed
  `indices_`、`RVVXYZFloatLayout<PointT>` 和 32-bit byte offset gate；`PointXYZ` board median 为
  `2.0989x`，`PointXYZI` board median 为 `1.5901x`，`PointXYZRGB` board median 为
  `1.6225x` 且 5/5 run 正向，`PointXYZRGBA` board median 为 `1.5528x` 但有 1/5 退化 warning。
- `getDistancesToModel` 当前测试专用候选为负向，不进入 production probe（生产探针）。
- QEMU 只证明 correctness（正确性）和日志形状；性能结论必须来自 board（板卡）或目标硬件。

## 默认恢复动作

当前未阻塞的生产收益扩展已基本关闭到内建代表点型：`PointXYZ`、`PointXYZI`、`PointXYZRGB`
和 `PointXYZRGBA` 都有接入后板卡数据。本轮整理 / review-ready 检查后可进入提交流程；
`030-sqrt-helper-audit` 只在出现 RVV sqrt/helper 或 dense-store 新候选时恢复，
自定义 registered xyz 点型只在先定义代表类型和测试边界后再启动。
