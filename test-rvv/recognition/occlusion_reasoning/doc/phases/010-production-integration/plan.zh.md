# Phase 010 Plan: production-integration

## 阶段意图和边界

本阶段把 Phase 000 中稳定正向的 `projection-filter-rvv` 候选接入
`ZBuffering::filter(model, indices, thres)` 真实 production（生产源码）入口，并用
production direct（真实生产路径证据）重跑 correctness、QEMU smoke（QEMU 正确性冒烟）、
asm attribution（反汇编归属）和 5-run board repeated（重复板卡性能测试）。用户本轮明确覆盖
PI5 决策：若接入后的板卡结果仍显示收益，可以自行把 patch 视为 adopted production behavior
（已采纳生产行为）并创建正式 `doc-rvv` 主题文档。

本阶段不修改公开 API，不优化 `getOccludedCloud()` 两个 free-function overload，不扩大到
smooth depth window，也不把 Phase 000 diagnostic 的 raw depth buffer 数据直接当 production
performance truth。`computeDepthMap()` 已有矩形 stride / 初始化修复补丁留在本阶段生产 patch
范围内验证，因为真实 `ZBuffering` 入口依赖该 depth buffer。

## 当前状态清单

| item | current state | path |
| --- | --- | --- |
| Phase 000 diagnostic | 5-run board repeated median `1.930x`，min/max `1.910x/1.940x`，checksum 一致 | `log/board/repeated_phase000_filter_diagnostic/summary.md` |
| Evidence Doctor | Errors=0，Warnings=0，Suggestions=1：缺少 taskset/governor/freq/temperature | `log/board/repeated_phase000_filter_diagnostic/evidence_doctor.md` |
| production source | `computeDepthMap()` 已有矩形 stride / fill 修复；`filter()` 尚无 RVV dispatch | `recognition/include/pcl/recognition/impl/hv/occlusion_reasoning.hpp` |
| point type strategy | production 入口是模板点类型；本阶段用 `RVVXYZAoSFloatLayout<ModelT>` 做 xyz AoS gate | `doc-rvv/rvv/RVV Generic Point Type Strategy.zh.md` |

## Phase Scope 与扩展队列

- `validated_scope`：`ZBuffering<ModelT, SceneT>::filter(model, indices, thres)`，
  `ModelT` 满足 `RVVXYZAoSFloatLayout<ModelT>`，`Scalar=float` xyz 字段，contiguous
  `PointCloud` 存储，raw `depth_` buffer 已由 `computeDepthMap()` 填充，输出为保序 indices。
- `unvalidated_scope`：free-function `filter()` / `getOccludedCloud()`，`filter(model, filtered)` 的
  `copyPointCloud` 端到端成本，smooth depth window 性能，非 xyz AoS 点类型，用户自定义点类型，
  scene 点类型泛型性能。
- `point_type_expansion_queue`：后续若要扩大，补 `PointXYZRGB` / `PointXYZI` / 自定义 xyz traits
  编译和 production direct board；本阶段只证明 `ModelT` xyz AoS gate 命中时的 dispatch。
- `phase_closeout_boundary`：只能关闭 `ZBuffering::filter(indices)` production direct 条目。

## 诊断到 Production 错配审计

| question | answer |
| --- | --- |
| evidence role | production direct |
| A/B boundary | public template member `ZBuffering::filter(model, indices, thres)` |
| 当前决策问题 | RVV-vs-scalar production adoption |
| diagnostic 是否可外推到 production | 只能作为进入 PI1 的理由；最终采纳必须看本阶段 production direct board |
| comparison-boundary / baseline mismatch 风险 | yes；Phase 000 raw depth helper 不含真实 `computeDepthMap()` 和成员状态 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 本轮 diagnostic 已 positive；若 production direct 弱/负/中性，则不采纳或转下一候选 |
| clean adoption 是否需要同一 production boundary RVV-vs-RVV detail A/B | 当前没有已采纳 RVV family；若本阶段再尝试 vcompress/detail family，需同边界 A/B |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `production-projection-filter-rvv` | model point stream + `depth_` buffer | `ModelT` xyz AoS, float fields | `ZBuffering::filter(indices)` | production path-hit + output compare | production bench | planned 5-run board repeated | production bench RVV symbol / inline helper | planned | planned | write RED production test |
| `projection-filter-vcompress` | same | same | production detail helper | not started | not started | not started | not started | not started | deferred | only after production direct result if more optimization remains |

## 实现和测试动作

| action | 产物 / 命令 | 完成判据 |
| --- | --- | --- |
| RED production path test | `src/test_occlusion_reasoning.cpp` | RVV build 因 production hook 未命中或符号缺失失败 |
| production helper / dispatch | `recognition/include/pcl/recognition/impl/hv/occlusion_reasoning.hpp` | `__RVV10__` + xyz AoS gate 走 RVV，否则自然回退标量 |
| correctness | `make -C test-rvv/recognition/occlusion_reasoning run_test_compare` | Std/RVV 都通过，RVV build 命中 production hook |
| QEMU smoke | `make -C test-rvv/recognition/occlusion_reasoning run_qemu_smoke` | 只证明 correctness 和路径，不写性能结论 |
| asm | `make -C test-rvv/recognition/occlusion_reasoning check_occlusion_filter_rvv_asm`，后续补 production asm gate | RVV 指令归属到 production bench hot path |
| board repeated | `board_repeated` 或 production-specific repeated target | 5-run decision bucket 稳定 |
| Evidence Doctor / registry | `record_evidence_state_repeated` 和 production registry | Error 为 0；Warning/Suggestion 已解释 |

## 板卡复跑预算和决策桶

默认 5-run，`BENCH_ARGS="65536 150 150 200 5"`。若 production direct median
`>= 1.20x` 且 `B/A < 1` 为 0，则为 `positive` 并按本轮用户授权采纳；`1.05x~1.20x` 为
`weak_positive`，只有实现很小且 Warning 可解释时采纳；`0.95x~1.05x` 为 `neutral`；`<0.95x`
为 `negative`；跨桶为 `unstable`，最多追加 1 组同边界复跑。

## 继续 / 停止条件

若 production direct 仍 positive，本阶段继续完成 adopted 文档：正式
`doc-rvv/recognition/occlusion_reasoning-RVV.zh.md`、topic-local evaluation、phase result、
matrix、roadmap 和 Handoff。若 production direct 非正向，保留或回滚需按证据角色说明；本轮用户只授权
“有收益即可采纳”，未授权负收益时自动回滚，所以负向时暂停报告。
