# Phase 050: adoption closeout plan

## 阶段意图和边界

本阶段把用户已确认采纳的 encode-only production patch（生产补丁）转成 adopted production behavior（已采纳生产行为）的文档状态。范围只覆盖 `io/include/pcl/compression/organized_pointcloud_conversion.h` 中两个 cloud encode overload：

- `OrganizedConversion<PointT, false>::convert(cloud, ...)`
- `OrganizedConversion<PointT, true>::convert(cloud, ...)`

本阶段不修改生产代码，不接入 decode overload，不修改 PNG/LZF 压缩后端，也不把 production direct conversion（转换直连）收益外推为完整 `OrganizedPointCloudCompression::encodePointCloud` 端到端收益。

## 当前状态清单

| area | 当前事实 | 路径 |
| --- | --- | --- |
| production patch | encode cloud overloads 已有 RVV dispatch，`__RVV10__` 下走 `convertCloudToDisparityRVV` / `convertCloudToDisparityColorRVV`，否则回退 Std helper | `io/include/pcl/compression/organized_pointcloud_conversion.h` |
| correctness | Std/RVV 9 个 gtest 全部通过，覆盖 diagnostic、decode v0 对拍和 production direct representative point types | `make run_test_compare` |
| asm | production probe 能在公开 overload 路径归属到 RVV 指令 | `make check_production_rvv_asm` |
| board production evidence | production direct 5-run 全部 checksum 一致；uncolored 约 2.01x-2.11x，colored RGB 约 1.35x-1.43x，mono 约 1.64x-1.77x | `log/board/production_direct_repeated/summary.md` |
| Evidence Doctor | production repeated manifest 为 `Errors=0, Warnings=1, Suggestions=0`；唯一 warning 是 mono case 组内离群，按 case 单独报告 | `log/board/production_direct_repeated/evidence_doctor.md` |
| rejected path | decode disparity -> cloud v0 正确但 0.85x-0.91x 退化，不接 production | `doc/phases/030-decode-backprojection/result.zh.md` |
| adoption decision | 用户已确认“板卡结果显示有收益即可接入”，因此本阶段可进入 S11 production closeout | 当前 goal |

## 假设与候选族

本阶段不新增 code shape。采纳的候选族是 `encode_production_probe_pointxyz_like`：用 `pcl::rvv::kRVVXYZAoSPointCompatible<PointT>` gate 证明 xyz 单 float AoS layout 后，RVV 分块读取 x/y/z、计算 disparity、按 `vfclass` 结果保留有限点，最后逐 lane 写出 disparity 和 RGB/mono。

`generic PointT` 的生产 gate 是 traits-based（基于字段特征），但本阶段板卡证据只覆盖 `PointXYZ`、`PointXYZI`、`PointXYZRGB`、`PointXYZRGBA`。自定义 xyz-like 点型、normal 复合点型、更多 RGB-like 布局和端到端压缩链路不在本阶段证明范围。

## 阶段优化矩阵

| candidate family | row source policy | point type / Scalar / layout | test | bench / board | asm | doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `encode_production_probe_pointxyz_like` | organized cloud order | `PointXYZ` / `PointXYZI` / `PointXYZRGB` / `PointXYZRGBA`, `float`, AoS xyz | `run_test_compare` pass | production direct repeated positive | `check_production_rvv_asm` pass | `Errors=0, Warnings=1` | adopt |
| `decode_disparity_backprojection_rvv_v0` | image scan order | `PointXYZ`, AoS output | correctness pass | negative diagnostic | asm pass | shared manifest decode Errors | rejected |
| full `encodePointCloud` end-to-end | organized compression pipeline | PNG/LZF backend included | not covered | not covered | not covered | not covered | next phase candidate |

## 实现和测试动作

| action | 产物 / 命令 | 完成判据 |
| --- | --- | --- |
| 创建 formal `doc-rvv` production 文档 | `doc-rvv/io/organized_pointcloud_conversion-RVV.zh.md` | 只写 adopted production 行为、dispatch/fallback、证据链和未覆盖范围 |
| 刷新 evaluation / roadmap / matrix / phase index / queue | topic-local docs 和 `doc-rvv/library-screening/io/io-function-evaluation-queue.zh.md` | 不再停在 PI5 待确认；改为 adopted with next-phase boundary |
| 补 topic-local doc suite | README、testing overview、correctness tests、benchmark/evidence、optimization evidence、code map | doc suite role inventory 不再有 unblocked doc-only 缺口 |
| 写 phase result | `result.zh.md` | 回填 Evidence Doctor warning、artifact tracking、continue / stop decision |
| 复跑验证 | `check_production_rvv_asm`、`run_test_compare`、`run_production_repeated_evidence_doctor`、`diff --check` | 无新增错误；若失败则修文档或降级结论 |

## Evidence Doctor 和 registry 规则

采用 `log/board/production_direct_repeated/evidence_manifest.json` 作为 production evidence 输入；Evidence Doctor 输出为 `log/board/production_direct_repeated/evidence_doctor.md` 和 `.json`。Raw logs 继续按 summary-only 策略留在本机，不默认提交。共享 `log/board/evidence_doctor.md` 的 decode Errors 仅支撑 decode v0 rejected，不参与 production adoption。

## 板卡复跑预算和决策桶

本阶段使用已经完成的 5-run repeated production direct summary。decision bucket 为 positive：所有 9 个 production direct case 的 min/median/max 都大于 1.0 且 checksum 一致；唯一 Warning 不改变 adoption，但要求 mono 与 RGB 分开报告。

若本阶段复跑验证改变 direction、Errors / Warnings 数量或 checksum，必须先刷新 summary、Evidence Doctor、evaluation、`doc-rvv` 和 queue，再重新判断 adoption。

## Doc suite role inventory

| role | 计划状态 | 路径 / 说明 |
| --- | --- | --- |
| topic_navigation | standalone | `README.zh.md` |
| testing_overview | standalone | `doc/testing-overview.zh.md` |
| correctness_tests | standalone | `doc/correctness-tests.zh.md` |
| benchmark_and_evidence | standalone | `doc/benchmark-and-evidence.zh.md` |
| optimization_evidence | standalone | `doc/optimization-evidence.zh.md` |
| optimization_roadmap | standalone | `doc/optimization-roadmap.zh.md` |
| test_support_code_map | standalone | `doc/test-support-code-map.zh.md` |
| phase_index | standalone | `doc/phases/README.zh.md` |
| evaluation_production | standalone | `doc/organized_pointcloud_conversion-evaluation.zh.md` |
| production_topic_doc | standalone | `doc-rvv/io/organized_pointcloud_conversion-RVV.zh.md` |

## 继续 / 停止条件

完成 adoption closeout 后仍需审计下一优化方向：

- 若完整 `OrganizedPointCloudCompression::encodePointCloud` 端到端 bench 尚未覆盖，且可在当前 topic 测试资产内完成，则创建下一 phase，优先验证局部 conversion 收益是否被 PNG/LZF 后端稀释。
- 若要扩大到更多泛型点型、自定义布局或 decode 新候选，需要独立 phase 和新的 correctness / asm / board / doctor 证据。
- 若 next actions 全部需要扩大到未授权 production 文件或其它 topic，或证据显示不值得继续，才允许停止并向用户说明。

## Diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | production-public：真实公开 overload 的 Std build 与 RVV build 对比 |
| A/B boundary | public overload；Std 侧使用 `convertCloudToDisparityStd` / `convertCloudToDisparityColorStd`，RVV 侧经公开 overload 命中 RVV helper |
| 当前决策问题 | RVV-vs-scalar adoption for encode conversion |
| diagnostic 是否可外推到 production | 诊断只提供升级理由；最终 adoption 以 production direct repeated 为准 |
| comparison-boundary / baseline mismatch 风险 | production repeated manifest 同边界；共享 single-run manifest 含 decode diagnostic，已分层处理 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 已完成 bounded production probe；decode negative 不允许接入 decode |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 不需要；当前不是新 family 替换已采纳 family，而是 public RVV path 对当前 scalar path 的首次 adoption |
