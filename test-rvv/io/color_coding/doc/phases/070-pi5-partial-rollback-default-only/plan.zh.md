# Phase 070 Plan: PI5 partial rollback default-only

## 阶段意图和边界

用户已在 PI5 checkpoint（生产接入第 5 步用户检查点）后授权部分回滚。本阶段只回答一个 production-public（真实公开入口）问题：移除 `encodeAverageOfPoints` 和 `encodePoints` 的 RVV 分流后，只保留 `setDefaultColor` RVV 是否仍有可保留收益。

本阶段覆盖：

- 生产文件：`io/include/pcl/compression/color_coding.h`。
- 公开入口：`ColorCoding<pcl::PointXYZRGBA>::setDefaultColor`。
- 保留标量入口：`encodeAverageOfPoints`、`encodePoints`、`decodePoints`。
- 点类型 / 布局：exact `pcl::PointXYZRGBA`，`rgba_offset == offsetof(pcl::PointXYZRGBA, rgba)`。
- 规模：production bench 当前只保留 `prod_set_default_color_4096`。

本阶段不证明：

- `encodeAverageOfPoints` 或 `encodePoints` 的新实现族。
- `decodePoints` 的生产接入。
- 完整 `OctreePointCloudCompression` 公开压缩入口收益。
- 泛型点类型、`PointXYZRGB` 或非标准 RGBA offset 的 RVV 收益。

## 当前状态清单

| item | current state | path |
| --- | --- | --- |
| Phase 060 PI5 result | encode average 负向，encode points 弱正 / 不稳定，default color 稳定正向；等待用户确认 | `doc/phases/060-pi2-production-patch-and-direct-evidence/result.zh.md` |
| production patch | 三条 RVV 分流已接入：encode average、encode points、default color | `io/include/pcl/compression/color_coding.h` |
| production direct tests | RVV 构建要求三条 hook 均命中 | `src/test_color_coding.cpp` |
| production bench | `--case-filter production` 当前包含 5 个 `prod_*` labels | `src/bench_color_coding.cpp` |
| board evidence | 5-run Phase 060 evidence 已登记，但包含将回滚的 encode labels | `log/board/production_repeat_5/*` |
| Evidence Doctor | Phase 060 为 Errors=2, Warnings=6, Suggestions=2 | `log/board/production_repeat_5/evidence_doctor.md` |

## 假设与候选族

| candidate family | hypothesis | expected evidence |
| --- | --- | --- |
| default-only production RVV | `setDefaultColor` 是连续 range store，RVV strided store 能形成净收益 | post-rollback production-public board median > 1.05x，且无 case-specific Error |
| encode RVV rollback | 移除负向 / 弱正 encode 分流后，生产源码只留下可解释且稳定的收益路径 | correctness 不变，bench production filter 不再报告 encode labels |

## 优化矩阵

| candidate family | row source policy | point type / layout | scope and entry | correctness / fallback target | bench / board target | asm boundary | Evidence Doctor | decision rule |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| production default color RVV | contiguous output range | exact `PointXYZRGBA` / RGBA offset | public `setDefaultColor` | `run_test_compare`；small range fallback；non-exact type scalar semantics | `run_board_color_coding_production_repeated` with `production` filter | `vsse32` in RVV bench asm | no Error for `prod_set_default_color_4096` | positive -> keep and document as adopted after user-authorized rollback |
| production encode average RVV | indexed color gather | exact `PointXYZRGBA` old gate | public `encodeAverageOfPoints` | semantic tests remain scalar | production labels removed from current evidence | not required after rollback | historical Phase 060 Error | rejected / rolled back |
| production encode points average RVV | indexed color gather + scalar diff stream | exact `PointXYZRGBA` old gate | public `encodePoints` | semantic tests remain scalar | production labels removed from current evidence | not required after rollback | historical Phase 060 Warning | rejected / rolled back |
| decode scalar | contiguous decode range | existing template semantics | public `decodePoints` | unchanged | not applicable | not applicable | not applicable | keep scalar |

## 实现和测试动作

| action | artifact / command | completion criterion |
| --- | --- | --- |
| 部分回滚 production encode RVV | `color_coding.h` | 公开 encode 入口直接调用 Std；删除 encode RVV helpers 和 encode hook；default RVV 保留 |
| 更新 production direct tests | `src/test_color_coding.cpp` | RVV 构建只要求 default hook 命中；encode 仅断言语义 |
| 更新 production bench evidence boundary | `src/bench_color_coding.cpp` | `--case-filter production` 只输出 `prod_set_default_color_4096` |
| 更新 registry doc refs | `Makefile` | Phase 070 board evidence 登记到当前 result / evaluation |
| 本地 / QEMU 验证 | `run_test_compare`、production smoke、`dump_bench_rvv` | Std/RVV correctness 通过，production smoke 输出 default label，asm 可归属 |
| 板卡验证 | `run_board_color_coding_production_repeated` | 5-run summary + manifest + Doctor + registry fresh |

## Evidence Doctor 和 registry 规则

输入：

- `log/board/production_repeat_5/summary.md`
- `log/board/production_repeat_5/evidence_manifest.json`
- `log/board/production_repeat_5/evidence_doctor.md`
- `log/evidence_registry.json`

处理：

- Error：修复或降级；不能用 Error 关闭 adopted。
- Warning：解释是否影响 default-only 保留结论。
- Suggestion：记录为后续 evidence 质量建议，不自动扩大优化范围。

## 板卡复跑预算和决策桶

- run count：5。
- 每 run：`--iterations 20 --warmup-iterations 3`。
- positive：median >= 1.05x 且 min >= 1.0x，无 case-specific Error。
- weak-positive：median 1.02x-1.05x 或存在偶发 < 1.0x。
- neutral / negative：median <= 1.02x 或大多数 runs below 1.0x。
- unstable：预算内 bucket 摇摆且 Doctor 指出长尾 / below-one 无法解释。

预算用尽后若 bucket 稳定，用该 bucket 决策；若不稳定，降级为不建议保留。

## Diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | `production_public` |
| A/B boundary | `public_overload`，真实 `ColorCoding<pcl::PointXYZRGBA>::setDefaultColor` |
| 当前决策问题 | default-only RVV production path 是否比 scalar production path 值得保留 |
| diagnostic 是否可外推到 production | 不使用 diagnostic 外推；本阶段重跑 production-public evidence |
| comparison-boundary / baseline mismatch 风险 | 不覆盖完整 octree compression；只覆盖 color coder public method |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 不适用；当前已在 production-public 边界 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 不需要；当前是从 no adopted RVV 到 default-only RVV 的 RVV-vs-scalar 决策 |

## 阶段完成条件

- `setDefaultColor` default-only production evidence 若 positive：保留 RVV，创建 / 刷新 `doc-rvv/io/color_coding-RVV.zh.md`，并把 topic 判断为 default-only adopted。
- 若 weak-positive、neutral、negative 或 Error 未解决：建议完整回滚 production patch，topic 进入 no-production 或 blocked decision。
- `encodeAverageOfPoints` 和 `encodePoints` 必须从当前 production patch 中移除；历史证据保留在 Phase 060。

## 继续 / 停止条件

继续条件：

- default-only evidence positive 且文档 / registry 尚未刷新。
- correctness、bench、asm 或 board 证据缺口仍在当前 topic 范围内。

停止条件：

- default-only evidence 已闭合，roadmap / matrix 中没有当前授权范围内值得继续的高优先级优化候选。
- 或 Evidence Doctor / 板卡 / dirty isolation 出现真实阻塞。

默认下一动作：执行部分回滚并重跑证据。
