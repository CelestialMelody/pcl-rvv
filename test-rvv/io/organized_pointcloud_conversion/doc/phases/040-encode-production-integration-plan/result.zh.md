# Phase 040 Result: encode-production-integration-plan

## 阶段结论

本阶段完成了 encode-only production integration loop（仅 encode 侧生产接入闭环）的 PI2-PI5 证据采集。生产补丁只接入
`OrganizedConversion<PointT, false>::convert(cloud, ...)` 和
`OrganizedConversion<PointT, true>::convert(cloud, ...)`；disparity/depth -> cloud 的 decode overload 维持标量。

当前 EvidenceDecision（证据决策）是 `pending_user_confirmation_adopt_production`：
production direct（真实生产入口直连）证据支持保留 encode RVV 补丁，但按 workflow 规则，PI5 后必须等待用户确认，不能由 worker 自行把 patch 视为 adopted production behavior（已采纳生产行为），也不能自行回滚。

## 实际执行范围

| 维度 | 本阶段已验证范围 | 未验证 / 未采纳范围 |
| --- | --- | --- |
| public entry（公开入口） | cloud -> disparity；cloud -> disparity + RGB/mono | disparity/depth -> cloud decode overloads |
| 点类型 | `PointXYZ`、`PointXYZI`、`PointXYZRGB`、`PointXYZRGBA` | 其它满足 `kRVVXYZAoSPointCompatible<PointT>` 的用户点类型和其它 PCL xyz-like 点型 |
| Scalar / layout | `float x/y/z`，AoS stride（数组结构跨步） | `double` 字段、非标准布局、未注册 xyz traits |
| 颜色路径 | RGB 和 mono 写出；`PointXYZRGB`、`PointXYZRGBA` | 其它带 color traits 的自定义类型 |
| 规模 gate | `cloud.size() >= 64` 走 RVV，否则 fallback | 小规模性能不做优化声明 |

## 计划动作回填

| action | 状态 | 证据 | 结论 |
| --- | --- | --- | --- |
| production direct RED | done | `make check_production_rvv_asm` 在生产补丁前失败，信息为 `[RED] production encode probe has no RVV instructions yet` | 生产路径探针能区分补丁前后 |
| 抽 Std helper | done | `io/include/pcl/compression/organized_pointcloud_conversion.h` | 原 cloud encode 标量主体抽成 `convertCloudToDisparityStd` / `convertCloudToDisparityColorStd` |
| 接 RVV helper | done | 同上；`pcl::rvv_load::strided_load3_f32m2`、`pcl::rvv::kRVVXYZAoSPointCompatible` | RVV 分流在 `__RVV10__` 下命中，不满足 gate 时自然 fallback |
| production direct correctness | done | `make run_test_compare`；board `run_test.log` | 9 个 gtest 通过，新增 `PointXYZI` / `PointXYZRGBA` production direct 与 Std helper 对拍 |
| production asm | done | `make check_production_rvv_asm` | production encode probe 含 `vlse32` / `vfclass` / `vfrdiv` 等 RVV 指令 |
| board smoke | done | `log/board/analyze_bench_compare.log`、`log/board/evidence_manifest.json` | 18 个 case checksum 均一致；production direct 单次均正向，decode diagnostic 仍负向 |
| production repeated board | done | `log/board/production_direct_repeated/summary.md` | 9 个 production public case 5-run 均正向 |
| Evidence Doctor | done | `log/board/evidence_doctor.md`、`log/board/production_direct_repeated/evidence_doctor.md` | 单次总 manifest 的 3 个 Error 只来自 rejected decode diagnostic；production repeated 为 Errors=0, Warnings=1 |

## Production direct 5-run 结果

| case | point type | evidence role | min | median | max | decision bucket |
| --- | --- | --- | ---: | ---: | ---: | --- |
| `production_pointxyz_disparity_dense_307k` | `PointXYZ` | production direct | 2.107x | 2.109x | 2.113x | positive |
| `production_pointxyz_disparity_mixed_invalid_307k` | `PointXYZ` | production direct | 2.070x | 2.071x | 2.073x | positive |
| `production_pointxyzi_disparity_dense_307k` | `PointXYZI` | production direct | 2.045x | 2.048x | 2.052x | positive |
| `production_pointxyzi_disparity_mixed_invalid_307k` | `PointXYZI` | production direct | 2.009x | 2.012x | 2.016x | positive |
| `production_pointxyzrgb_disparity_rgb_dense_307k` | `PointXYZRGB` | production direct | 1.367x | 1.370x | 1.378x | positive |
| `production_pointxyzrgb_disparity_mono_dense_307k` | `PointXYZRGB` | production direct | 1.637x | 1.690x | 1.765x | positive |
| `production_pointxyzrgb_disparity_rgb_mixed_invalid_307k` | `PointXYZRGB` | production direct | 1.357x | 1.382x | 1.387x | positive |
| `production_pointxyzrgba_disparity_rgb_dense_307k` | `PointXYZRGBA` | production direct | 1.395x | 1.412x | 1.427x | positive |
| `production_pointxyzrgba_disparity_rgb_mixed_invalid_307k` | `PointXYZRGBA` | production direct | 1.346x | 1.362x | 1.382x | positive |

所有 production direct case 的 Std/RVV checksum 一致。`PointXYZRGB` mono 的 median 1.690x 高于同组 RGB case，Evidence Doctor 报 `group_outlier` warning；解释是 mono 只写 1 字节/点，RGB 写 3 字节/点，不能把 mono 收益外推到 RGB。

## Evidence Doctor 处理

| 输入 | 结果 | 处理 |
| --- | --- | --- |
| `log/board/evidence_manifest.json` | Errors=3, Warnings=28 | 3 个 Error 均为 `pointxyz_decode_disparity_*` 的 B/A 退化；decode v0 已在 Phase 030 rejected，不参与 production adoption。Warnings 包含 diagnostic mask boundary mismatch、单次 run 和 mono group outlier。 |
| `log/board/production_direct_repeated/evidence_manifest.json` | Errors=0, Warnings=1 | 唯一 warning 是 mono 高收益离群；按 RGB/mono 分开报告，不外推。 |

## Diagnostic 到 production 边界回填

| question | answer |
| --- | --- |
| evidence role | encode diagnostic 是 `diagnostic`；本阶段 production bench 是 `production_direct`。 |
| A/B boundary | diagnostic 是 test helper；production repeated 是 public overload（公开入口）Std build vs RVV build。 |
| 当前决策问题 | 当前只回答 public RVV path 是否快于 public scalar path，不回答新 RVV family 是否优于其它已采纳 RVV family。 |
| diagnostic 是否可外推到 production | 不能直接外推。Phase 040 用 production direct repeated 证据重新闭合。 |
| comparison-boundary / baseline mismatch 风险 | diagnostic 有 mask 实现差异 warning；production direct manifest 使用公开有限值语义并由 checksum 对齐。 |
| weak / negative diagnostic 是否允许 bounded production probe | encode diagnostic positive 且实现范围可控，允许 bounded probe；decode diagnostic negative，不进入 production。 |
| clean adoption 是否需要 RVV-vs-RVV detail A/B | 当前没有既有 adopted RVV family，因此不需要 family selection A/B；但 PI5 后仍需要用户确认采纳。 |

## 继续 / 停止判断

本阶段的 code、test、asm、board repeated 和 Evidence Doctor 已闭合到 `pending_user_confirmation_adopt_production`。合法停止条件是 `production_adoption_requires_user_authorization`：worker 不能自行把生产补丁升级为 adopted，也不能写 `doc-rvv/io/organized_pointcloud_conversion-RVV.zh.md` 作为长期采纳文档。

若用户确认保留 / 采纳，下一步进入 S11 production closeout（生产收尾文档），补长期 `doc-rvv`、提交边界和最终 Handoff。若用户不确认采纳或要求回滚，需要明确授权回滚后再撤回 production patch。
