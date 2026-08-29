# Phase 000 Plan: current-state-and-response-grid-production-probe

## 阶段意图和边界

本阶段验证 `TrajkovicKeypoint2D::detectKeypoints()` 中 response grid（响应图）的生产 RVV
路径是否值得接入。范围冻结为：

- 入口：真实 `pcl::TrajkovicKeypoint2D<pcl::PointXYZI, pcl::PointXYZI>::compute()`。
- 方法：FOUR_CORNERS 和 EIGHT_CORNERS。
- 点类型 / `Scalar` / 布局：`PointXYZI`，强度字段为单个 `float`，organized AoS（结构数组）布局。
- 窗口：`window_size == 3`。其它窗口回退标量。
- 输入形态：organized、无 indices（源码 `initCompute()` 已拒绝 indices）、完整点云。
- RVV 覆盖：仅计算内部像素的 `response_`。NMS 排序、occupancy map、输出构造保持标量。
- 不证明：泛型 `IntensityT`、RGB 灰度 accessor、`PointXYZINormal`、其它窗口、非 organized 输入和 NMS RVV 化。

用户本轮授权覆盖：如果板卡 production direct（真实生产入口直连）结果显示收益，即可采纳当前 production patch，
并进入 production closeout 与 `doc-rvv/keypoints/trajkovic_2d_response_grid-RVV.zh.md` 创建。

## 当前状态清单

| 对象 | 状态 | 路径 |
| --- | --- | --- |
| production 源码 | 尚未接 RVV；response grid 全标量 | `keypoints/include/pcl/keypoints/impl/trajkovic_2d.hpp` |
| topic 测试资产 | 本阶段新建 | `test-rvv/keypoints/trajkovic_2d_response_grid/` |
| evaluation | 本阶段新建 | `doc/trajkovic_2d_response_grid-evaluation.zh.md` |
| optimization matrix | 本阶段新建 | `doc/phases/optimization-matrix.zh.md` |
| roadmap | 本阶段新建 | `doc/optimization-roadmap.zh.md` |
| `doc-rvv` 长期文档 | production 采纳前不适用 | `doc-rvv/keypoints/trajkovic_2d_response_grid-RVV.zh.md` |

## 假设与候选族

| candidate family | 假设 | 风险 |
| --- | --- | --- |
| `response-grid-rvv` | 每行内部像素按 VL chunk（可变向量长度分块）跨步加载 intensity，批量计算 4/8 方向响应并写回 `response_`，能减少逐点算术与 `std::vector<float>` 临时对象成本。 | EIGHT_CORNERS 标量路径的 `min(D)` 写法需要严格复刻；除零、阈值和 NaN 行为必须同构。 |
| `nms-scalar-tail` | NMS 保持标量，避免改变排序、occupancy 和 OpenMP critical 输出顺序。 | response-only 收益可能被 NMS 稀释。 |
| `generic-intensity-traits` | 后续可扩展到其它带 float intensity 的点型。 | 当前阶段先用 `PointXYZI` exact scope 闭合 production evidence，不能外推成泛型。 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| response-grid-rvv | organized grid internal pixels | `PointXYZI` / float intensity / AoS / `window_size == 3` | production public `compute()` | `run_test_compare` 覆盖 4/8 corners、tail width、fallback window | `run_board_trajkovic_2d_repeated` | planned 5-run | `check_production_rvv_asm` | planned manifest | planned | 实现生产 RVV helper 后验证 |
| fallback-scalar | organized grid | 非 RVV 构建、非 `PointXYZI`、非默认 accessor、非 3x3 window | production public `compute()` | `run_test_compare` 中 std/RVV 输出一致 | not_applicable | not_applicable | not_applicable | not_applicable | planned | fallback 测试必须闭合 |
| nms-scalar-tail | sorted full indices | `PointXYZI` | production public `compute()` | keypoint indices 和 output intensity 对拍 | 包含在 production bench | planned | 标量 tail 无 RVV 归属要求 | planned | planned | 保持标量 |

## 实现和测试动作

| 动作 | 产物 | 命令 / 证据 | 完成判据 |
| --- | --- | --- | --- |
| 写 correctness test（正确性测试） | `src/test_trajkovic_2d_response_grid.cpp` | `make run_test_compare` | std/RVV 两个构建均通过；RVV 构建真实命中 production helper |
| 写 production bench | `src/bench_trajkovic_2d_response_grid.cpp` | 板卡 `run_board_trajkovic_2d_repeated` | 四个 case checksum 一致并产出 repeated summary |
| 接入 production helper | `keypoints/include/pcl/keypoints/impl/trajkovic_2d.hpp` | `check_production_rvv_asm` | RVV 构建出现 `trajkovic2DResponseGridRVV` 和关键 RVV 指令 |
| 生成 Evidence Doctor manifest | `script/generate_trajkovic_2d_evidence_manifest.py` | `run_board_evidence_doctor` | Errors=0；Warnings 必须解释或降级 |
| 更新文档 | phase result、evaluation、roadmap、Handoff；若采纳则更新 `doc-rvv` | `git diff --check`、`evidence_status` | 文档与当前证据路径一致 |

## Evidence Doctor 和 registry 规则

manifest 路径为 `log/board/evidence_manifest.json`，doctor 报告为 `log/board/evidence_doctor.md`，
registry 为 `log/evidence_registry.json`。若只生成 Markdown summary 而 metadata 不完整，Evidence Doctor 结论降级为
`metadata_incomplete`，不得 clean-adopt。

## 板卡复跑预算和决策桶

默认 5-run。`mean` 和 `median` 均大于 1.05x 且 `B/A < 1` 频率不高于 1/5 时为 positive；
1.00x 到 1.05x 为 weak-positive，需要实现小、fallback 简单且 asm 质量闭合才采纳；跨 1.0 反复摇摆为
unstable。本轮若首次 repeated summary 已稳定落桶，不额外无限复跑。

## diagnostic-to-production mismatch audit

本阶段直接使用 production-public evidence role（真实公开入口证据角色）和 public overload A/B boundary
（公开重载对比边界），不是未接 production 的 diagnostic。对比问题是 RVV-vs-scalar；baseline 和 candidate
只差 `__RVV10__` 分流，计时包含 response grid、NMS、输出构造和真实 `compute()` wrapper。该证据只证明
当前 `PointXYZI` / 3x3 organized public path 是否快于标量，不证明泛型点型或其它窗口。

## 文档更新清单

- `doc/phases/000-current-state-and-response-grid-production-probe/result.zh.md`
- `doc/phases/optimization-matrix.zh.md`
- `doc/optimization-roadmap.zh.md`
- `doc/trajkovic_2d_response_grid-evaluation.zh.md`
- `README.zh.md`
- 若 production evidence positive 并采纳：`doc-rvv/keypoints/trajkovic_2d_response_grid-RVV.zh.md`
- 当前 Handoff：`tmp/rvv-work-logs/keypoints/trajkovic_2d_response_grid/current-handoff/current-handoff.zh.md`

## 继续 / 停止条件

继续条件：correctness、asm 或 board 仍缺同边界证据，且工具 / 板卡可用。

停止条件：板卡不可达且 SSH_AUTH_SOCK 注入后仍失败；Evidence Doctor Error 无法修复；production direct 性能为
negative / unstable；或当前 phase 已闭合且 roadmap 无本轮授权内未阻塞动作。

下一阶段默认入口：Phase 000 的实现与证据闭环；若 positive 并采纳，则进入 production closeout。
