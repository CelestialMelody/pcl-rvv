# Phase 000 Result: current-state-and-response-grid-production-probe

## 当前结论

Phase 000 已闭合。`TrajkovicKeypoint2D<PointXYZI, PointXYZI>::compute()` 的
EIGHT_CORNERS response grid（响应图）已按本轮授权采纳为窄范围 production RVV
路径；FOUR_CORNERS 保持标量 fallback（回退路径），只作为 checksum（校验和）和
额外开销控制项记录。

本阶段只关闭以下范围：`PointXYZI`、默认 `IntensityFieldAccessor`、organized full-cloud
public `compute()`、`window_size == 3`、EIGHT_CORNERS、单 float intensity 字段、AoS
（结构数组）布局。NMS（非极大值抑制）、其它点型、RGB intensity accessor、非 3x3
window 和非 RVV 构建仍走原标量路径。

## 计划动作回填

| 动作 | 状态 | 命令 / 证据 | 结论 |
| --- | --- | --- | --- |
| production RVV helper | done | `keypoints/include/pcl/keypoints/impl/trajkovic_2d.hpp` | 新增 `PointXYZI` / 默认 accessor / 3x3 / EIGHT_CORNERS gate；RVV 只写 response grid，NMS 保持标量。 |
| correctness（正确性） | done | `make -C test-rvv/keypoints/trajkovic_2d_response_grid run_test_compare` | Std 与 RVV 构建各 4 个 gtest 全通过；测试覆盖 FOUR_CORNERS、EIGHT_CORNERS、tail width 和非 3x3 fallback。 |
| asm attribution（反汇编归属） | done | `make -C test-rvv/keypoints/trajkovic_2d_response_grid check_production_rvv_asm` | helper 被编译器内联到 `detectKeypoints()`，反汇编中存在 `vlse32.v`、`vfmul.vv`、`vfmin.vv`、`vfdiv.vv`、`vmfge.vf` 和 masked `vse32.v`。 |
| board repeated（板卡重复性能测试） | done | `SSH_AUTH_SOCK=$SSH_AUTH_SOCK make -C test-rvv/keypoints/trajkovic_2d_response_grid run_board_trajkovic_2d_repeated` | Milkv-Jupiter 5-run；EIGHT_CORNERS 两个规模 checksum 全匹配且稳定正向。 |
| Evidence Doctor（证据体检） | done | `log/board/evidence_doctor.md` | EIGHT_CORNERS production-public comparisons 为 Errors=0 / Warnings=0 / Suggestions=0。 |
| evidence registry（证据登记） | done | `make -C test-rvv/keypoints/trajkovic_2d_response_grid evidence_status` | registry fresh；summary、manifest 和 doctor 已登记。 |

## 板卡证据

当前证据路径：

- summary：`test-rvv/keypoints/trajkovic_2d_response_grid/log/board/repeated-summary.md`
- manifest：`test-rvv/keypoints/trajkovic_2d_response_grid/log/board/evidence_manifest.json`
- doctor：`test-rvv/keypoints/trajkovic_2d_response_grid/log/board/evidence_doctor.md`
- registry：`test-rvv/keypoints/trajkovic_2d_response_grid/log/evidence_registry.json`

| case | role | runs | mean std ms | mean rvv ms | mean B/A | median B/A | checksum |
| --- | --- | ---: | ---: | ---: | ---: | ---: | --- |
| `eight_corners_320x240` | production-public | 5 | 50.232920 | 29.147240 | 1.725x | 1.743x | match |
| `eight_corners_641x481_tail` | production-public | 5 | 264.472800 | 183.589400 | 1.442x | 1.450x | match |
| `four_corners_320x240` | production-fallback-control | 5 | 29.871220 | 29.467840 | 1.014x | 1.000x | match |
| `four_corners_641x481_tail` | production-fallback-control | 5 | 202.176400 | 200.361200 | 1.009x | 1.003x | match |

`B/A = Std build ms / RVV build ms`，大于 1 表示 RVV 构建更快。FOUR_CORNERS 的
near-1x 数字不用于 RVV 采纳判断，因为 RVV 构建按 production gate 回退标量；它只证明回退控制项
没有 checksum 分叉，且额外分流成本接近噪声范围。

## Evidence Doctor 解释

本轮先前板卡 run 出现 checksum mismatch，已降级为 historical evidence（历史证据），不参与采纳。
修正 correctness 后重新跑 5-run，并把 manifest 拆成 EIGHT_CORNERS production-public comparison
和 FOUR_CORNERS fallback control。新版 Evidence Doctor 只对真实采纳候选做 production-public 检查，
结果为 Errors=0 / Warnings=0 / Suggestions=0。

## Optimization Matrix 更新

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| response-grid-rvv | organized grid internal pixels | `PointXYZI` / float intensity / AoS / 3x3 | production public `compute()` / EIGHT_CORNERS | passed `run_test_compare` | board repeated production bench | positive：1.725x / 1.442x mean B/A | passed; inlined `detectKeypoints()` RVV instructions | Errors=0 | adopted | none for this frozen scope |
| fallback-scalar | organized grid | FOUR_CORNERS、非 RVV 构建、非 `PointXYZI`、非默认 accessor、非 3x3 window | production public `compute()` | passed fallback/control cases | fallback control in board summary | checksum match, near 1x control | not_applicable | not_applicable | adopted as scalar fallback | none for this frozen scope |
| nms-scalar-tail | sorted full indices | `PointXYZI` | production public `compute()` | keypoint/output 对拍通过 | included in production bench | EIGHT_CORNERS end-to-end 仍 positive | scalar tail | not_applicable | adopted as scalar tail | no current need to RVV 化 |

## 继续 / 停止判断

`continue_stop_decision`：stop for current topic closeout。当前冻结范围已经有 correctness、asm、
board repeated、Evidence Doctor 和 registry 证据；roadmap 中仍有点型扩展、RGB accessor、大窗口和
NMS 方向，但它们会扩大到未验证点型、输入语义或新 production gate，不属于本阶段可以无风险继续采纳的范围。

`stop_condition_hit`：当前矩阵没有本阶段授权范围内的未阻塞高优先级动作。后续若要继续，应新建 point-type
expansion 或 larger-window phase，并重新完成 correctness、asm、board 和 Evidence Doctor。

## Doc Suite Role Inventory

| role | status | 说明 |
| --- | --- | --- |
| topic_navigation | `standalone:README.zh.md` | 已更新当前 adopted 状态和命令。 |
| testing_overview | `merged:doc/trajkovic_2d_response_grid-evaluation.zh.md#测试计划和 bench 计划` | 当前 topic 规模较窄，测试入口和证据边界在 evaluation 中可恢复。 |
| correctness_tests | `merged:doc/trajkovic_2d_response_grid-evaluation.zh.md#测试计划和 bench 计划` | gtest 名称、输入和证明范围已在 evaluation 中说明。 |
| benchmark_and_evidence | `merged:doc/trajkovic_2d_response_grid-evaluation.zh.md#验证结果` | board summary、manifest、doctor 和 registry 路径已列出。 |
| optimization_evidence | `standalone:doc/phases/optimization-matrix.zh.md` + `doc/optimization-roadmap.zh.md` | 已更新 adopted / deferred 状态。 |
| optimization_roadmap | `standalone:doc/optimization-roadmap.zh.md` | 已写后续扩展恢复条件。 |
| test_support_code_map | `merged:doc/trajkovic_2d_response_grid-evaluation.zh.md#Traceability Map` | 当前测试支撑只有 `src/` 与 `script/`，不需要再拆 internal header。 |
| phase suite | `standalone:doc/phases/**` | Phase 000 plan/result/matrix/index 已闭合。 |
| evaluation_production | `standalone:doc/trajkovic_2d_response_grid-evaluation.zh.md` | 记录最终生产证据和接入判断。 |
| production_topic_doc | `standalone:doc-rvv/keypoints/trajkovic_2d_response_grid-RVV.zh.md` | 本轮按用户授权采纳 production patch 后创建。 |

## 未覆盖范围与恢复条件

- 泛型 intensity 点类型尚未覆盖。当前 production gate 是具体 `PointXYZI` 和默认 accessor；若要覆盖
  `PointXYZINormal` 或其它 single-float intensity 点型，需要先按泛型点类型策略补 traits / offset /
  layout gate、fallback tests、asm 和板卡证据。
- RGB 灰度 accessor 尚未覆盖。它不是单 float intensity 跨步加载，不能复用本阶段证据。
- `window_size > 3` 尚未覆盖。更大窗口的邻域 offset 和公式成本不同，需要单独 phase。
- NMS RVV 化不建议作为下一步默认动作。EIGHT_CORNERS end-to-end 已稳定正向，当前没有 profile
  证明 NMS 是采纳后瓶颈；同时排序、occupancy 和输出顺序风险较高。
