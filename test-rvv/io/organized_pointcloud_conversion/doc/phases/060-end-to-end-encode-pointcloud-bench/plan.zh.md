# Phase 060: end-to-end encodePointCloud bench plan

## 阶段意图和边界

本阶段验证 adopted conversion patch 是否能在 encodePointCloud-shaped helper（形态模拟 `encodePointCloud` 的测试专用 helper）中保持实际收益。这里的 end-to-end（端到端）边界包含：

1. `analyzeOrganizedCloud`
2. `OrganizedConversion<PointT>::convert`
3. PNG encode
4. stream write

本阶段只改 `test-rvv/io/organized_pointcloud_conversion` 测试资产、manifest wrapper 和 topic 文档，不改 production code。当前无 OpenNI cross build 无法直接实例化真实 `OrganizedPointCloudCompression` 类，因此本阶段证据角色是 production-shaped diagnostic（生产形态诊断），不是 production-public（真实公开入口）证据。若端到端收益弱或负向，本阶段只降级“形态接近入口的收益”结论，不回滚已经 adopted 的 conversion-only production patch。

## 当前状态清单

| area | 当前事实 |
| --- | --- |
| conversion direct | production repeated 9 cases 全部 positive，Doctor `Errors=0, Warnings=1` |
| end-to-end caller | 未覆盖；现有 fixtures 为 `height=1`，不能直接喂给 `analyzeOrganizedCloud` |
| board | 用户说明板卡可用；本 phase 需要板卡 repeated 或至少 board smoke 后再写性能结论 |
| docs | 050 已补 formal `doc-rvv`，但明确 full `encodePointCloud` deferred |

## 假设与候选族

候选族是 `end_to_end_encode_pointcloud_bench`，它不是新 RVV helper，而是 production-shaped caller evidence（生产形态调用方证据）。假设：

- 若 PNG / stream 写出不是绝对主成本，RVV conversion 仍应给 `encodePointCloud` 带来可观收益。
- 若 PNG / stream 或 `analyzeOrganizedCloud` 主导，则端到端 speedup 会显著低于 conversion-only；此时下一步应先做 component ablation，而不是继续扩大 RVV production gate。

## 阶段矩阵

| candidate family | point type / input | correctness | bench / board | doctor | decision |
| --- | --- | --- | --- | --- | --- |
| `end_to_end_encode_pointcloud_bench` | `PointXYZ`, 640x480 organized finite | planned smoke + checksum | planned board repeated | planned | pending |
| `end_to_end_encode_pointcloud_bench` | `PointXYZRGB`, 640x480 organized finite, RGB | planned smoke + checksum | planned board repeated | planned | pending |
| `end_to_end_encode_pointcloud_bench` | `PointXYZRGB`, 640x480 organized finite, mono | planned smoke + checksum | planned board repeated | planned | pending |

## 实现和测试动作

| action | 产物 / 命令 | 完成判据 |
| --- | --- | --- |
| 新增 organized-dimension fixture | `include/impl/opc_fixtures.hpp` | `width * height == size` 且 `height > 1` |
| 新增 encodePointCloud smoke TEST | `src/test_organized_pointcloud_conversion.cpp` | 输出 stream 非空；Std/RVV build 均通过 |
| 新增 bench cases | `src/bench_organized_pointcloud_conversion.cpp` | case-filter 可隔离 `production_full_*` labels，checksum 为 compressed stream bytes |
| 更新 manifest metadata | `script/generate_opc_board_evidence_manifest.py` | Evidence Doctor 能识别 full encode labels 为 production-shaped end-to-end |
| 更新 docs / phase result | topic docs | 记录端到端与 conversion-only 证据边界 |
| 运行验证 | `run_test_compare`、board repeated / doctor、`diff --check` | 无 correctness failure；性能结论来自 board |

## Evidence Doctor 和 registry 规则

新增 labels 使用 `production_full_*` 前缀。它们是 production-shaped diagnostic，`timer_boundary` 必须写成 `encodePointCloud_end_to_end_checksum_after_timing`，不能和 conversion-only case 混成同一 group。Checksum policy 是 compressed stream byte checksum；若 Std/RVV PNG 输出 byte-for-byte 不一致但 decode 语义一致，本阶段先把 evidence 降级为 blocked，因为当前 smoke 没有 decode oracle。

## 板卡复跑预算和决策桶

默认先跑 5-run repeated，沿用现有 production repeated 统计口径：

- `positive`：所有 end-to-end case median > 1.05x，且 checksum 一致。
- `weak_positive`：median > 1.0x 但至少一个 case 接近 1.0x 或 warning 明显。
- `neutral_or_negative`：median <= 1.0x，或 Evidence Doctor Error 未解。
- 若只有单次 board smoke，本阶段只能记录 smoke，不写端到端性能结论。

## 继续 / 停止条件

若端到端仍 positive，保留 current adoption，并考虑是否继续 `analyzeOrganizedCloud` 或 color pack component A/B。若端到端 neutral / negative，应暂停进一步 production 扩展，说明 conversion patch 是局部加速但完整压缩入口受后端稀释。若 board 或 tooling 不可用，写 blocked result；当前用户已说明板卡可用，因此不能以“需要板卡”早停。

## Diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | production-shaped diagnostic |
| A/B boundary | encodePointCloud-shaped helper |
| 当前决策问题 | adopted conversion patch 是否改善形态接近真实压缩 caller 的链路 |
| diagnostic 是否可外推到 production | 不能直接外推；本 phase 复刻 production pipeline 关键步骤，但不是 public class entry |
| comparison-boundary / baseline mismatch 风险 | PNG output bytes、stream write、focalLength analyze 都进入计时；manifest 需单独标记 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 本阶段不是 production-public probe；结果只影响后续扩展建议 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 不需要；不是新 RVV family selection |
