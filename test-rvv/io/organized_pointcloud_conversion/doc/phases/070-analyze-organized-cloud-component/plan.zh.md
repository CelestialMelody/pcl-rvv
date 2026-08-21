# Phase 070: analyzeOrganizedCloud component plan

## 阶段意图和边界

本阶段做 `analyzeOrganizedCloud` 组件消融（component ablation，单独测量流水线中的一个组件），判断完整 encode-shaped 端到端收益被稀释后，是否值得继续对 `analyzeOrganizedCloud` 做 production probe（生产探针）。

本阶段只修改 `test-rvv/io/organized_pointcloud_conversion` 的测试资产、bench case、manifest metadata 和 topic 文档，不修改 production code。当前真实 `OrganizedPointCloudCompression` 类在无 OpenNI cross build 中无法直接实例化，因为头文件成员 `openni_wrapper::ShiftToDepthConverter` 受 `HAVE_OPENNI` 条件影响；因此本阶段证据角色是 diagnostic（诊断），不能直接写成 production direct（真实生产入口直连）。

## 当前状态清单

| area | 当前事实 |
| --- | --- |
| adopted conversion | cloud encode overload 已采纳，production repeated 9 cases 全部正向，Doctor `Errors=0, Warnings=1` |
| full encode-shaped | 060 的 production-shaped helper 5-run 中位数约 1.15x，Doctor `Errors=0, Warnings=0` |
| analyze helper | 只在 `encodePointCloud` 前段标量执行，当前没有独立 correctness / board 消融 |
| board | 用户说明板卡可用；本阶段需要板卡 repeated 才能写性能结论 |

## 假设与候选族

候选族是 `analyze_cloud_max_depth_rvv`。标量路径逐点扫描 organized cloud，遇到更大的有限 `z` 时更新 `maxDepth` 并重新计算 focal length。RVV 候选先用向量跨步加载 `x/y/z` 和 `vfclass` 找到最大有限 `z` 的下标，再用该点按原公式标量计算 focal length。

这个形态只改变“如何找最大深度点”，不改变最终 focal length 公式。若输入中最大 `z` 唯一，最终下标应与标量路径一致；若多个点有相同最大 `z`，候选必须保持标量的 first-wins（第一次出现胜出）语义。

## 阶段矩阵

| candidate family | point type / input | correctness | bench / board | doctor | decision |
| --- | --- | --- | --- | --- | --- |
| `analyze_cloud_max_depth_rvv` | `PointXYZ`, 640x480 organized finite | planned same-chain test | planned board repeated | planned | pending |
| `analyze_cloud_max_depth_rvv` | `PointXYZ`, 640x480 organized mixed invalid | planned same-chain test | planned board repeated | planned | pending |

## 实现和测试动作

| action | 产物 / 命令 | 完成判据 |
| --- | --- | --- |
| 新增 analyze checksum | `include/impl/opc_types.hpp` | max depth 和 focal length bit pattern 可进入 bench checksum |
| 新增 analyze diagnostic helper | `include/impl/opc_candidates.hpp` | Std/RVV 输出与 scalar same-chain 一致 |
| 新增 correctness TEST | `src/test_organized_pointcloud_conversion.cpp` | finite 与 mixed invalid organized cloud 通过 |
| 新增 bench cases | `src/bench_organized_pointcloud_conversion.cpp` | `--case-filter analyze_*` 可隔离组件消融 |
| 更新 manifest metadata | `script/generate_opc_board_evidence_manifest.py` | analyze labels 被标为 diagnostic component |
| 跑验证和板卡 | `run_test_compare`、board repeated、Evidence Doctor | checksum 一致，性能结论只来自板卡 |

## Evidence Doctor 和 registry 规则

新增 labels 使用 `analyze_*` 前缀，证据角色是 `diagnostic`，A/B boundary 是 `test_helper`，timer boundary 是 `analyze_checksum_after_timing`。若 analyze case 正向，只能说明该组件值得进入 bounded production probe；是否接入 production 还需要后续 phase 处理 `HAVE_OPENNI` 构建边界、真实类入口或生产 detail helper 证据。

## 板卡复跑预算和决策桶

默认跑 5-run repeated，`--iterations 30 --warmup-iterations 5 --case-filter analyze_*`。

- `positive`：两个 case median > 1.10x，checksum 一致，Doctor 无 Error。
- `weak_positive`：median > 1.0x 但低于 1.10x，或有 warning 需要解释。
- `neutral_or_negative`：median <= 1.0x，或 Doctor Error 未解。

## 继续 / 停止条件

若 analyze 消融 positive，本 topic 仍有未阻塞下一阶段：创建 bounded production probe plan，先解决无 OpenNI 构建下真实入口不可实例化的问题，或把生产 detail helper 抽到可测边界。若 neutral / negative，拒绝 analyze production 扩展，保留当前 encode-only conversion adoption，并只把 analyze 写成 attempted / rejected。

## Diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | diagnostic |
| A/B boundary | test helper |
| 当前决策问题 | `analyzeOrganizedCloud` 组件是否值得进入 production probe |
| diagnostic 是否可外推到 production | 不能直接外推；公式来自 production helper，但当前没有真实 `OrganizedPointCloudCompression` public entry |
| comparison-boundary / baseline mismatch 风险 | 计时边界只含 analyze，不含 PNG / conversion / stream；positive 只能说明组件潜力 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 弱或负向时默认不继续；只有 profile 显示 analyze 是实际热点才重开 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 需要后续 production boundary 证据；本阶段不 clean-adopt |
