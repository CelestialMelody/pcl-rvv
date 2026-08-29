# Phase 000 Plan: current-state and diagnostic scaffold

## 阶段意图和边界

本阶段把 `keypoints/include/pcl/keypoints/impl/harris_2d.hpp` 的第一 RVV 目标收窄为 organized derivative + response map（有组织导数与响应图）production-shaped diagnostic（生产形态诊断）。范围覆盖 `detectKeypoints()` 中强度导数、`computeSecondMomentMatrix()` 局部累加和 `responseHarris/Noble/Lowe/Tomasi()` 四类响应公式；不覆盖 NMS sort（非极大值抑制排序）、occupancy map（占用图）、OpenMP critical 输出、`refineCorners()` 或 production dispatch（生产分流）。

## 当前状态清单

| 项目 | 当前事实 | 路径 |
| --- | --- | --- |
| source | `HarrisKeypoint2D` 是 header-only 模板入口，要求 organized cloud 且不支持 subset indices | `keypoints/include/pcl/keypoints/harris_2d.h`、`keypoints/include/pcl/keypoints/impl/harris_2d.hpp` |
| scalar path | production 先计算 `derivatives_rows_` / `derivatives_cols_`，再按 response method 逐点计算响应 | `harris_2d.hpp` |
| test assets | 当前 topic 原本没有 `test-rvv/keypoints/harris_2d` scaffold | 本阶段新建 |
| board availability | `test-rvv/config.mk` 已有 board 变量；本阶段需要板卡 repeated bench 才能给性能结论 | `REMOTE_USER`、`REMOTE_IP`、`BOARD_LABEL` |
| production state | 当前不修改 production，先筛选 candidate 是否值得进入 PI1 | production patch not yet started |

## 假设与候选族

| candidate family | 假设 | 风险 | 本阶段状态 |
| --- | --- | --- | --- |
| `response-map-rvv` | 先把 `IntensityT` 访问结果摊成连续 float image，RVV 接管内部像素导数、窗口累加和响应公式 | second-moment 是小窗口逐像素规约，窗口边界和 Tomasi `sqrt` 需要严格对拍 | planned |
| `direct-IntensityT-rvv` | 在 production 中直接对点类型字段做跨步或 traits load | 泛型 intensity accessor 和点类型 layout 尚未审计 | deferred |
| `nms-rvv` | 对 threshold / occupancy map 继续优化 | sort、状态写入和 critical push 顺序敏感 | rejected for this phase |

## 诊断到 production 错配审计

| question | answer |
| --- | --- |
| evidence role | `production_shaped_diagnostic` |
| A/B boundary | `test_helper`：`computeResponsesScalar()` vs `computeResponsesCandidate()` |
| 当前决策问题 | RVV-vs-scalar diagnostic，判断 response map 子链路是否值得进入 production integration loop |
| diagnostic 是否可外推到 production | unknown；它复刻公式和输入形状，但未经过 public `HarrisKeypoint2D::detectKeypoints()` dispatch |
| comparison-boundary / baseline mismatch 风险 | yes；bench 不包含 NMS、sort、output push 和真实 `IntensityT` 访问成本 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | weak_positive 可在实现小且 fallback 简单时允许；neutral/negative 不进入 production probe |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | no；当前没有已有 adopted RVV family，若进入 production 只需 production public Std/RVV 证据验证 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `response-map-rvv` | organized full image, no indices | test synthetic float intensity image / float / contiguous | `computeResponsesCandidate()` test helper | `run_test_compare` Std/RVV gtest | `bench_harris_2d` three response-map cases | planned 5-run board repeated if QEMU + asm pass | `bench_harris_2d_rvv` RVV float instructions | planned | planned | write RED test first |
| `direct-IntensityT-rvv` | organized full image, no indices | production template point type | public `HarrisKeypoint2D` | not run | not run | not run | not run | not run | deferred | only after diagnostic positive |
| `nms-rvv` | sorted response indices | output point vector | NMS branch | not run | not run | not run | not run | not run | rejected for this phase | do not optimize in this phase |

## 实现和测试动作

| action | 产物 / 命令 | 完成判据 |
| --- | --- | --- |
| RED test | `src/test_harris_2d.cpp` / `make -C test-rvv/keypoints/harris_2d run_test_rvv` | RVV build 因 candidate 未命中 RVV path 失败 |
| scalar reference | `include/impl/harris_2d_candidates.hpp` | 复刻导数、窗口上界、finite gate 和四类公式 |
| RVV candidate | `include/impl/harris_2d_candidates.hpp` | `__RVV10__` 下命中 RVV path 且对拍 scalar reference |
| bench harness | `src/bench_harris_2d.cpp` | 输出 case、iterations、checksum 和 vector_chunks |
| asm | `make -C test-rvv/keypoints/harris_2d check_harris_2d_rvv_asm` | 反汇编出现预期 RVV float 指令 |
| board repeated | `board_repeated record_evidence_state_repeated` | 5-run summary、manifest、Evidence Doctor 和 registry 生成 |

## 板卡复跑预算和决策桶

默认 5-run，每个 case 运行 `--iterations 20 --warmup-iterations 3`。`median >= 1.20x` 且 `B/A < 1 = 0` 为 positive；`1.05x~1.20x` 且最多 1 次低于 1 为 weak_positive；`0.95x~1.05x` 为 neutral；`< 0.95x` 为 negative；预算内方向摇摆为 unstable。

## Phase Scope 与扩展队列

| 字段 | 内容 |
| --- | --- |
| validated_scope | synthetic organized full-image response map，float intensity，Harris/Noble/Lowe/Tomasi，3x3 和 5x5 configured window |
| unvalidated_scope | public `HarrisKeypoint2D` dispatch、真实 `IntensityT` 点类型访问、NMS、indices、非 organized 输入、更多 point type / intensity accessor |
| point_type_expansion_queue | 若 diagnostic positive，下一 phase 先做 PI1：审计 `PointXYZI` 或常用 intensity accessor，补 production direct test、fallback、asm 和 board |
| phase_closeout_boundary | 只能关闭 response-map diagnostic；不能关闭 production generic template 入口 |

## 继续 / 停止条件

只要 RED、GREEN、QEMU correctness、asm、board repeated、Evidence Doctor 或 registry 仍未闭合且没有真实 blocker，本阶段继续推进。若板卡不可达、QEMU/工具链失败、Evidence Doctor Error 无法解释、或 diagnostic board 为 neutral/negative，则停止在 diagnostic closeout，不进入 production。

## 文档更新清单

本阶段更新 README、evaluation、testing overview、correctness tests、benchmark/evidence、optimization evidence、test-support code map、phase index、optimization matrix、optimization roadmap 和 current handoff。当前不创建 `doc-rvv/keypoints/harris_2d-RVV.zh.md`；只有 production patch 经接入后板卡证据显示有收益，本轮 prompt override 才允许采纳并创建长期文档。
