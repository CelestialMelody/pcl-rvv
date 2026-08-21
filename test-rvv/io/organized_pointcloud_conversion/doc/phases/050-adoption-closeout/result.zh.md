# Phase 050: adoption closeout result

## 当前结论

用户已确认采纳 encode-only production patch。本阶段把 `io/include/pcl/compression/organized_pointcloud_conversion.h` 中的 `OrganizedConversion<PointT>::convert(cloud, ...)` RVV 路径记录为 adopted production behavior（已采用生产行为）。

采用范围只覆盖 cloud -> disparity / RGB / mono 的 public overload；disparity/depth -> cloud decode overload 保持标量。完整 `OrganizedPointCloudCompression::encodePointCloud` 只在后续 phase 验证，不属于本阶段采纳边界。

## 计划回填

| action | 状态 | 证据 / 路径 | 结果 |
| --- | --- | --- | --- |
| 保留 production patch | completed | `io/include/pcl/compression/organized_pointcloud_conversion.h` | Std/RVV helper 和 `__RVV10__` dispatch 保留 |
| 刷新长期文档 | completed | `doc-rvv/io/organized_pointcloud_conversion-RVV.zh.md` | 已写当前采用方式、fallback、证据链和未覆盖范围 |
| 刷新 topic-local docs | completed | `README.zh.md`、`doc/*.zh.md`、`doc/phases/*` | adopted 状态与后续 phase 边界已同步 |
| 同步筛选队列 | completed | `doc-rvv/library-screening/io/io-function-evaluation-queue.zh.md` | 队列状态从 PI5 待确认更新为已采纳 / 继续验证 |

## 证据链

| 证据 | 路径 / 命令 | 结果 | 边界 |
| --- | --- | --- | --- |
| correctness（正确性） | `make run_test_compare` | Std/RVV 对拍通过 | 输出语义一致，不证明性能 |
| asm（反汇编） | `make check_production_rvv_asm` | production probe 含 RVV 指令 | 证明路径命中 |
| production repeated board | `log/board/production_direct_repeated/summary.md` | 9 个 case 全部正向 | 只覆盖 conversion public overload |
| Evidence Doctor（证据体检） | `log/board/production_direct_repeated/evidence_doctor.md` | `Errors=0, Warnings=1, Suggestions=0` | mono group outlier 已解释 |

## Evidence Doctor 处理

唯一 warning 是 `production_pointxyzrgb_disparity_mono_dense_307k` 的 group outlier。解释是 mono path 每点写 1 byte，RGB path 每点写 3 bytes，计时边界不同；因此 mono 与 RGB 分开报告，不把 mono 收益外推到 RGB。该 warning 不阻塞 adoption。

共享 single-run Doctor 中的 decode Errors 只用于支撑 decode v0 rejected，不参与 encode adoption。

## Doc suite parity

| area | current shape scan | quality bar | decision | evidence | next action |
| --- | --- | --- | --- | --- | --- |
| README navigation | 已有 topic 入口和命令 | 需要说明当前结论与证据白名单 | adopted | `README.zh.md` | 随后续 phase 刷新 |
| testing overview | 已拆出测试分层 | 需要 target 粒度审计 | adopted | `doc/testing-overview.zh.md` | 随新增 target 刷新 |
| correctness tests | 已说明 gtest family | 需要覆盖 production direct / fallback 语义 | adopted | `doc/correctness-tests.zh.md` | 随新增 TEST 刷新 |
| benchmark/evidence | 已说明 production repeated | 需要 Doctor 和提交边界 | adopted | `doc/benchmark-and-evidence.zh.md` | 060 / 070 后刷新 |
| optimization evidence | 已按 candidate family 建表 | 需要 adopted / rejected / deferred 状态 | adopted | `doc/optimization-evidence.zh.md` | 060 / 070 后刷新 |
| test support code map | 已拆 `src`、`include`、`include/impl`、`script` | 需要能定位 helper 和 bench | adopted | `doc/test-support-code-map.zh.md` | 随新增 helper 刷新 |
| production topic doc | 已创建长期 `doc-rvv` | 只保存 adopted production 行为 | adopted | `doc-rvv/io/organized_pointcloud_conversion-RVV.zh.md` | 060 / 070 后补证据边界 |

## 继续 / 停止判断

本阶段不命中停止条件。当前 adoption 已闭合，但 roadmap 仍有未阻塞动作：验证完整 encode-shaped 压缩链路是否仍受益。因此默认下一阶段是 `060-end-to-end-encode-pointcloud-bench`。
