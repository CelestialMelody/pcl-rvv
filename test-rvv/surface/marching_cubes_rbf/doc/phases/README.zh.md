# marching_cubes_rbf phase index

| phase | 状态 | 默认恢复入口 | 说明 |
| --- | --- | --- | --- |
| `000-current-state-and-component-ablation` | completed | `result.zh.md` | component ablation（组件消融）诊断正向，已升级到 production integration（生产接入）。 |
| `010-production-integration-pointnormal` | adopted_production_behavior | `result.zh.md` | exact `pcl::PointNormal` production patch 已由用户确认保留；QEMU / 板卡 correctness（正确性）通过，board production direct 三个 case 为弱正向。 |
| `020-generic-normal-point-types` | adopted_production_behavior | `020-generic-normal-point-types/result.zh.md` | production gate 已扩展为 `RVVXYZNormalFloatLayout<PointNT>`；`PointNormal`、`PointXYZINormal`、`PointXYZRGBNormal` 代表点型已通过 correctness、asm、board production bench 和 Evidence Doctor。 |

当前 production patch（生产补丁）已成为 traits-gated normal AoS（结构数组）生产行为。正式长期文档为
`doc-rvv/surface/marching_cubes_rbf-RVV.zh.md`，其中记录当前采纳范围、fallback（回退路径）矩阵、
板卡 production direct 数据和未覆盖范围。

默认下一动作：若 reviewer 接受 weak-positive（弱正向）证据，可进入提交准备；若需要强稳定性能证据，
创建 `030-production-repeated-values-summary`，改造 bench / analyzer 输出逐次 B/A values 后再重跑板卡。
