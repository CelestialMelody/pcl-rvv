# Phase 010 Plan: production-scope-audit

## 阶段意图和边界

本阶段把 Phase 000 的 post-projection single polygon diagnostic（投影后单多边形诊断）升级为更接近真实 `segment` 扫描段的 full-scan diagnostic（完整扫描段诊断）：高度判断改用真实平面系数 `a*x+b*y+c*z+d`，polygon predicate（多边形谓词）从 `projected_points` 按 `k1/k2` 选择二维坐标，输出仍保持 `indices` 顺序。

本阶段只修改 `test-rvv/segmentation/extract_polygonal_prism_data/**`。不修改 `segmentation/include/pcl/segmentation/impl/extract_polygonal_prism_data.hpp`，不新增 production dispatch（生产分流）。

## 当前状态清单

| area | 当前状态 | 证据路径 |
| --- | --- | --- |
| Phase 000 candidate | 只覆盖水平平面，height mask 等价为 `z` 范围 | `include/impl/eppd_candidates.hpp` |
| Phase 000 board evidence | post-projection scan board 5-run median 3.56x，Evidence Doctor 0/0/0 | `log/board/repeated/summary.md`、`log/board/repeated/evidence_doctor.md` |
| production source | `segment` 仍为纯标量，先 `projectPoints`，再用 `pointToPlaneDistanceSigned` 和 `projected_points[k1/k2]` 扫描 | `segmentation/include/pcl/segmentation/impl/extract_polygonal_prism_data.hpp` |
| 未闭合项 | 任意平面、`projected_points` 坐标选择、arbitrary indices、concave hull 多 polygon 和泛型点类型 | `doc/phases/optimization-matrix.zh.md` |

## 假设与候选族

| candidate family | 假设 | 风险 |
| --- | --- | --- |
| full-scan single polygon RVV | 在已经完成 `projectPoints` 后，逐点高度距离和二维 polygon predicate 仍可用 RVV 批量处理 | 需要同时加载 input point 和 projected point，访存多于 Phase 000；倾斜平面浮点边界更敏感 |
| production dispatch probe | 若 full-scan diagnostic 正确且板卡仍 positive，可进入 PI1 生产接入计划 | 继续到 PI2 会修改 production，需要用户授权 |
| arbitrary indices gather | full-scan dense 通过后再扩展 source-indexed 路径 | gather 成本和 32-bit byte offset gate 需要单独证据 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | test | bench | board | asm | doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| full-scan single polygon RVV | dense ordered indices | `PointXYZ` / `float` / arbitrary plane coefficients / runtime `k1/k2` | 新增倾斜平面 correctness | 更新 `bench_eppd` 或新增 mode | repeated board | `segmentPolygonalPrismRvvFullScanCandidate` 下 RVV 指令 | manifest + doctor | 本阶段待定 |
| production patch | public `segment` | template `PointT` | not_run | not_run | not_run | not_run | not_run | turn_stop_deferred until user authorizes PI2 |

## 实现和测试动作

| action | 产物 | 预期证据 | 完成判据 |
| --- | --- | --- | --- |
| RED：新增倾斜平面 full-scan test | `src/test_eppd.cpp` | `make run_test_rvv` 失败，因为 candidate 不存在或仍按 `z` 判断 | 失败原因指向 full-scan helper 缺失 / 语义未实现 |
| GREEN：新增 full-scan reference / RVV helper | `include/impl/eppd_reference.hpp`、`include/impl/eppd_candidates.hpp` | `make run_test_compare` pass | 倾斜平面结果与手写 expected / reference 一致 |
| bench：让 bench 覆盖 full-scan path | `src/bench_eppd.cpp` | QEMU smoke checksum 一致 | bench 输出仍可由 manifest wrapper 解析 |
| asm：检查 RVV 归属 | `make dump_bench_rvv` | full-scan helper 下出现 RVV load / mask / compress 指令 | 不把 QEMU timing 当性能 |
| board：有界复跑 | `make run_board_eppd_repeated BENCH_ARGS="--size 262144 --iterations 8 --warmup 2"` | repeated summary + Evidence Doctor | decision bucket 稳定或降级 |

## Evidence Doctor 和 registry 规则

本阶段继续使用 topic-local wrapper `script/generate_eppd_board_evidence_manifest.py` 生成 `log/board/repeated/evidence_manifest.json`，再调用 `../../script/evidence_doctor.py`。若 bench label 改为 full-scan，应同步更新 wrapper 的 case role 和 summary 文案。

当前 topic 还没有 `log/evidence_registry.json`；恢复和提交前用路径限定 `git status --short --untracked-files=all -- test-rvv/segmentation/extract_polygonal_prism_data` 做人工 freshness check（新鲜度检查）。

## 阶段完成条件

- full-scan helper 通过 correctness、QEMU smoke、asm attribution（反汇编归属）、board repeated 和 Evidence Doctor。
- 若 full-scan board 仍 positive：保持 `partial-production-candidate`，下一步是请求用户授权 production integration loop。
- 若 full-scan board 退化或 Evidence Doctor 报警未能解释：降级为 diagnostic，并把 production patch 保持 blocked。

## 板卡复跑预算和决策桶

预算为 5-run repeated collection，参数沿用 Phase 000：`--size 262144 --iterations 8 --warmup 2`。若 Evidence Doctor 出现 warning 或 direction 接近 1.0，最多追加一次同边界复跑；若桶仍摇摆，标为 `unstable` 并停止自动复跑。

默认 decision bucket：median speedup 明显大于 1 且 min/max 同向为 positive；接近 1 或跨方向为 neutral / unstable；小于 1 为 negative。

## 继续 / 停止条件

本阶段完成后，如果 production integration 仍是唯一 high-priority next action，则命中 `production_authorization_boundary` 并停止等待用户确认。若仍有不触碰 production 的 high-priority diagnostic 缺口，例如 full-scan correctness 未闭合或 wrapper 解析失败，则继续在本 topic 内修复。

## 文档更新清单

更新 `doc/phases/010-production-scope-audit/result.zh.md`、`doc/phases/README.zh.md`、`doc/phases/optimization-matrix.zh.md`、`doc/optimization-roadmap.zh.md` 和 `doc/extract_polygonal_prism_data-evaluation.zh.md`。未进入 PI5 前不创建 `doc-rvv` production 长期主题文档。

## diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | production-shaped diagnostic |
| A/B boundary | topic-local full-scan helper，仍不是 public overload |
| 当前决策问题 | RVV-vs-scalar，以及是否值得请求 production integration |
| diagnostic 是否可外推到 production | partial：扫描段语义更接近 production，但仍不包含 `projectPoints`、对象状态和 public dispatch |
| comparison-boundary / baseline mismatch 风险 | yes：bench 比较 helper，不比较真实 `segment` |
| weak / negative 时是否允许 bounded production probe | 只有能解释为 helper 边界差异且 PI1 可冻结小补丁时才允许；否则保持 diagnostic |
| clean adoption 是否需要 production boundary A/B | yes：PI2-PI5 必须补真实 public entry correctness、fallback、asm 和 board |

## phase scope 与扩展队列

`validated_scope`：`PointXYZ`、`float`、dense ordered indices、single polygon、任意平面系数和 runtime `k1/k2`。

`unvalidated_scope`：arbitrary indices gather、concave hull 多 polygon RVV、其它 `PointT` / PointXYZ-like traits、`Scalar=double`、完整 `segment` 入口收益和 production dispatch。

`point_type_expansion_queue`：若生产接入被授权，先读泛型点类型策略，优先尝试 `RVVXYZAoSFloatLayout<PointT>` gate；若 gate 不能闭合，则仅允许具体点型阶段性接入并要求 fallback 测试。
