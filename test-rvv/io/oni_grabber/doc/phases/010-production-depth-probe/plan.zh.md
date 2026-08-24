# Phase 010 Plan: production-depth-probe

## 阶段意图和边界

本阶段进入 production integration loop（生产接入闭环）的 PI1-PI5，范围冻结为 `io/src/oni_grabber.cpp` 中 `ONIGrabber::convertToXYZPointCloud` 的 depth-only `PointXYZ` production-detail helper（生产内部 helper）。本阶段不改变公开 API，不覆盖 RGB/RGBA、IR、真实 ONI 文件读取、replay reader 调度、public-entry throughput（公开入口吞吐）或 `Scalar=double`。

`validated_scope`：`PointXYZ`、`float`、organized contiguous depth map、depth-only projection、invalid depth mask、`__RVV10__` RVV build 与非 RVV fallback。

`unvalidated_scope`：RGB/RGBA/IR、尺寸不一致 resize 成本、ONI reader / device wrapper、真实 ONI 文件吞吐、其它点类型和 public-entry dispatch。

## PI1 gate

| gate | decision |
| --- | --- |
| production 文件 | 只允许修改 `io/src/oni_grabber.cpp`。 |
| helper 形态 | 复用 `openni2_grabber` 的 production-detail 结构：源码顶部新增 `fillXYZPointCloudStd` / `fillXYZPointCloudRVV` / `fillXYZPointCloudCandidate` 和 test hook。 |
| fallback | 非 `__RVV10__` 构建自然走 Std；RVV build 只对该 helper 短路到 RVV。 |
| semantic boundary | 保持原 `centerX = width >> 1`、`centerY = height >> 1` 和 `u/v` 从负中心到正中心前一格的语义。 |
| public API | 不新增公开入口、不改 ONIGrabber 类声明。 |
| PI5 停止 | 无论 production-detail 证据正负，PI5 后停在用户检查点，不自动采纳或回滚。 |

## Diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | `production-detail`（生产内部边界证据），不是 public-entry。 |
| A/B boundary | production detail helper；Std/RVV 通过 test hook 和 bench 的 `prod_xyz_depth_full_640x480` case 对比。 |
| 当前决策问题 | `RVV-vs-scalar` production-detail 是否值得保留。 |
| diagnostic 是否可外推到 production | Phase 000 已证明 synthetic helper 正向；本阶段只外推到生产内部 helper，不外推到完整 ONI replay。 |
| comparison-boundary / baseline mismatch 风险 | public-entry 仍有 reader / device wrapper mismatch；production-detail 内部 A/B 边界可闭合。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 本阶段就是 bounded production probe；若结果不支持，PI5 停在待用户确认回滚。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 当前没有已采纳 ONI RVV family；若 production-detail Std/RVV positive 且 fallback / asm / doctor 通过，可作为待用户确认采纳候选。 |

## 实现和测试动作

| action | artifact / command | expected evidence | done condition |
| --- | --- | --- | --- |
| 红灯测试 | `src/oni_grabber_production_detail_test.cpp` + `make run_test_compare` | 缺少 production hook 时链接失败 | 已观察到预期失败 |
| PI2 production patch | `io/src/oni_grabber.cpp` | helper 抽出，public method 调用 candidate | production diff 局限在 depth-only helper |
| PI3 correctness | `make run_test_compare` | diagnostic + production-detail gtest 通过 | Std/RVV 均通过 |
| QEMU bench smoke | `make run_bench_rvv BENCH_ARGS="--case-filter prod_xyz_depth_full_640x480 --iterations 2 --warmup-iterations 1"` | 日志形状，不是性能 | 输出 checksum |
| asm | `make check_oni_grabber_production_rvv_asm` | hook 符号与关键 RVV 指令存在 | grep gate 通过 |
| board repeated | `make collect_board_oni_grabber_production_repeated` + summary | 目标硬件 production-detail 性能 | 5-run decision bucket |
| Evidence Doctor | `make run_board_oni_grabber_production_evidence_doctor` | Errors / Warnings / Suggestions | 写入 result / Handoff |

## Continue / Stop

继续条件：PI1 gate 闭合且本阶段只触碰 `io/src/oni_grabber.cpp`、当前 topic 测试资产和 topic-local 文档。停止条件：production hook 无法在当前交叉环境编译、correctness 失败、asm 不闭合、板卡不可达、Evidence Doctor Error 无法降级、或 production-detail repeated 结果需要用户判断。
