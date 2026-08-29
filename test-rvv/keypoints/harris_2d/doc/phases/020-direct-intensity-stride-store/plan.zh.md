# Phase 020 Plan: direct intensity stride store

## 阶段意图和边界

本阶段只尝试一个 production（生产源码）实现形态优化：`responseRVV()` 内部像素原本先写入连续
`std::vector<float>`，最后再逐点拷回 `PointOutT::intensity`。本阶段改为对 interior pixels（内部像素）
用 RVV `vsse32` 跨步存储直接写回输出点的 intensity 字段，边界像素仍走标量 `computeSecondMomentMatrix()`。

本阶段不扩大 public API（公开接口）、不改变 `HarrisKeypoint2D::compute()` 调用方式、不覆盖 NMS
（非极大值抑制）、不新增 indices / non-organized 输入支持，也不把当前 `PointXYZI` 公开入口证据外推成
完整泛型点类型结论。

## 当前状态清单

| 项目 | 当前事实 | 路径 |
| --- | --- | --- |
| production patch | 已有 `responseRVV()`，在 `input_->is_dense` 时由四类 response helper 分流进入 | `keypoints/include/pcl/keypoints/impl/harris_2d.hpp` |
| public-entry baseline | clean timer 5-run 板卡结果三组 case 均大于 1.0，但整体是 weak_positive（弱正向） | `test-rvv/keypoints/harris_2d/log/board/repeated_phase010_public_entry_clean_timer/summary.md` |
| Evidence Doctor | phase010 public-entry baseline 为 Errors=0、Warnings=0、Suggestions=7 | `test-rvv/keypoints/harris_2d/log/board/repeated_phase010_public_entry_clean_timer/evidence_doctor.md` |
| correctness | public direct gtest 已对 Harris/Noble/Lowe/Tomasi 与标量 reference 对拍 | `test-rvv/keypoints/harris_2d/src/test_harris_2d.cpp` |

## 候选族和假设

| candidate family | 假设 | 风险 | 本阶段动作 |
| --- | --- | --- | --- |
| `direct-intensity-stride-store` | 省掉 full image `responses` 初始化和末尾 `for` 拷贝，公开入口 weak speedup 可小幅改善 | `PointOutT::intensity` 是 AoS（结构数组）字段，跨步写需要保持字段 offset 和 stride 正确 | 采用窄范围实现，使用 `offsetof(PointOutT, intensity)` 和 `sizeof(PointOutT)`，并用现有 public direct test / asm / board 验证 |
| `direct-IntensityT-load` | 直接从 `input_` 通过 traits 或 accessor 向量化读取 intensity | 需要泛型 `IntensityT` accessor 和点类型字段策略，当前会扩大范围 | deferred，进入后续 point type / accessor expansion phase |
| `NMS-rvv` | 继续优化排序和 occupancy 后处理 | 输出顺序和临界区语义敏感，当前 public-entry 证据尚未指向 NMS 为主瓶颈 | rejected for this phase |

## 诊断到 production 错配审计

| question | answer |
| --- | --- |
| evidence role | `production-public` |
| A/B boundary | `public overload`：`HarrisKeypoint2D<PointXYZI, PointXYZI>::compute()` |
| 当前决策问题 | RVV-vs-scalar；判断当前 production RVV patch 是否比同一公开入口标量构建更快 |
| diagnostic 是否可外推到 production | phase000 diagnostic 只作为进入 production probe 的依据；phase020 采纳与否以 public-entry board evidence 为准 |
| comparison-boundary / baseline mismatch 风险 | phase010 脚本标签仍需从 `production_shaped_diagnostic/test_helper` 修正为 `production-public/public overload`；本阶段会修正并重建 evidence |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 用户本轮允许“板卡显示有收益即可采纳”；若 5-run 全 case `B/A < 1 = 0` 且 Evidence Doctor 无 Error/Warning，可按 weak-positive 采纳，但文档必须写清弱收益边界 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | no；当前没有已有 adopted RVV family，本阶段只比较 public scalar build 和 public RVV build |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `direct-intensity-stride-store` | organized full image, no indices | `PointXYZI -> PointXYZI` public entry / float / AoS intensity field | `HarrisKeypoint2D::compute()` with `setNonMaxSupression(false)` | `make run_test_compare` | board `--public-entry` clean timer | planned 5-run repeated | `bench_harris_2d_rvv.asm` must show RVV float stores | planned | planned |
| `direct-IntensityT-load` | organized full image, no indices | broader `IntensityT` / point type traits | not changed | not_run | not_run | not_run | not_run | not_run | deferred |

## 实现和测试动作

| action | 产物 / 命令 | 完成判据 |
| --- | --- | --- |
| evidence label fix | `script/generate_harris_2d_evidence_manifest.py`、`Makefile` | public-entry run 可生成 `production-public/public overload` manifest |
| implementation | `keypoints/include/pcl/keypoints/impl/harris_2d.hpp` | interior RVV response 直接跨步写回 intensity，边界标量写回保持语义 |
| correctness | `make -C test-rvv/keypoints/harris_2d run_test_compare` | Std/RVV QEMU gtest 均通过 |
| asm | `make -C test-rvv/keypoints/harris_2d check_harris_2d_rvv_asm` | 反汇编包含 RVV float response 指令和 store 指令 |
| board repeated | `board_repeated record_evidence_state_repeated` with tag `phase020_direct_intensity_stride_store_public_entry` | 5-run summary、manifest、Evidence Doctor 和 registry 生成 |

## 板卡复跑预算和决策桶

本阶段沿用 5-run、每 case `--iterations 20 --warmup-iterations 3 --public-entry`。原配置桶仍记录
`positive >= 1.20x`、`weak_positive 1.05x~1.20x`、`neutral 0.95x~1.05x`、`negative < 0.95x`；
但 production adoption（生产采纳）按本轮 prompt override：若三组 public-entry case 均大于 1.0、
`B/A < 1 = 0`、Evidence Doctor 无 Error/Warning，允许采纳并创建长期 `doc-rvv`。

## Phase Scope 与扩展队列

| 字段 | 内容 |
| --- | --- |
| validated_scope | `PointXYZI -> PointXYZI`，organized dense input，public `compute()`，Harris/Noble/Tomasi bench case，Lowe 由 correctness 覆盖 |
| unvalidated_scope | broader `PointOutT` intensity layout、其它 `IntensityT` accessor、non-dense fallback 性能、NMS、indices、其它点类型 |
| point_type_expansion_queue | 若后续扩大到泛型点类型，需要基于 `RVV Generic Point Type Strategy.zh.md` 审计 intensity 字段 traits、POD layout、offset、fallback test 和 dedicated board bench |
| phase_closeout_boundary | 只关闭当前 public-entry `PointXYZI` production path；不关闭泛型 `PointOutT` 或 NMS |

## 继续 / 停止条件

QEMU、asm、板卡 repeated、Evidence Doctor 或 registry 未闭合时继续推进。若新实现导致 correctness 失败、
反汇编无法归属、板卡出现任一 case 反向或 Evidence Doctor Error，则停止并保留生产 patch 供用户 / reviewer 判断；
不自动回滚用户可见 production 改动。

## 文档更新清单

本阶段完成后更新 phase020 result、optimization matrix、optimization roadmap、evaluation、
benchmark/evidence、optimization evidence、README、current handoff。若 production public evidence 满足本轮采纳条件，
创建 `doc-rvv/keypoints/harris_2d-RVV.zh.md`，并以 phase020 post-integration board data 为正式数据来源。
