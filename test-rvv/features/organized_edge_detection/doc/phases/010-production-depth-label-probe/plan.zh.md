# Phase 010 Production Depth Label Probe Plan

## 阶段意图和边界

本阶段把 Phase 000 的 depth label diagnostic（深度标签诊断）候选接入
`features/include/pcl/features/impl/organized_edge_detection.hpp` 的真实 production（生产源码）路径，并用
production direct（真实生产入口直连）证据判断是否采纳。用户已授权：如果接入后的板卡 repeated
benchmark（重复性能测试）显示有收益，可以采纳，并用接入后的板卡数据创建正式 `doc-rvv` 文档。

本阶段只接管 `OrganizedEdgeBase<PointT, PointLT>::extractEdges()` 中 depth discontinuity（深度突变）
主路径。RGB Canny（RGB 边缘检测）和 normal Canny（法线边缘检测）前处理不在本阶段优化范围内；派生类
只会因为调用 base depth path 而间接命中本阶段的 depth RVV 分流。

## 当前状态清单

| 项 | 当前事实 |
| --- | --- |
| Phase 000 | `test-rvv/features/organized_edge_detection` 已有 test-only `computeDepthLabelsRVV()`。 |
| correctness（正确性） | `run_test_compare` Std/RVV 各 3/3 pass。 |
| board diagnostic（板卡诊断） | `log/board/repeated-summary.md`：finite 320x240 mean `4.446x`，tail 641x481 mean `4.617x`，NaN boundary mean `3.075x`。 |
| Evidence Doctor（证据体检） | `0E/0W/3S`，Suggestion 为缺少 taskset/governor/freq/temperature。 |
| production 状态 | `organized_edge_detection.hpp` 尚未修改；没有 production direct 证据。 |
| 队列表 | 已更新为 `partial-production-candidate`，等待 PI1 授权。 |

## PI1 范围冻结

| question | answer |
| --- | --- |
| evidence role | `production-public` + `production-detail`。 |
| A/B boundary | public `OrganizedEdgeBase::compute()` / `extractEdges()`，并用 bench wrapper 直接调用 production header。 |
| 当前决策问题 | RVV-vs-scalar：接入后的真实 production RVV path 是否快于当前 production scalar path。 |
| diagnostic 是否可外推到 production | 只能作为候选价值信号；最终采纳只看本阶段 production direct 证据。 |
| comparison-boundary / baseline mismatch 风险 | Phase 000 helper 不含 `PointT` traits、`PointLT` 输出布局和真实 `assignLabelIndices()`；本阶段必须重新测试。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 已有 diagnostic 强正向且用户授权；若 production board 退化，则不采纳并保留回滚/改形态判断。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 当前 production 无既有 RVV family；只需 Std/RVV public A/B。 |

## Phase Scope 与扩展队列

| 字段 | 范围 |
| --- | --- |
| validated_scope | `PointT` 满足 PCL traits z-field 单 `float`、AoS 布局可按 stride 读取；`PointLT` 暂时收窄为 `pcl::Label`；organized cloud 全量 `width * height`；labels 与 `label_indices` 顺序等价。 |
| unvalidated_scope | 非 `pcl::Label` 的 `PointLT`、非 traits z-field 点类型、RGB/normal Canny 前处理、`assignLabelIndices()` RVV 化、indices/subset 入口。 |
| point_type_expansion_queue | PI5 后若收益成立，单独补 `PointXYZI` / `PointXYZRGB` / `PointXYZRGBNormal` 等 PointXYZ-like smoke 与 board；`PointLT` 泛型 label field traits 另列 phase。 |
| phase_closeout_boundary | 只能关闭 depth production path；不能关闭 RGB/normal 派生前处理或泛型 `PointLT`。 |

## 实现和测试动作

| 动作 | 产物 / 命令 | 完成判据 |
| --- | --- | --- |
| RED asm gate | 新增 production bench/asm target，先在未改 production 时运行 | 目标失败，说明当前 production 符号没有 RVV 指令归属。 |
| PI2 production patch | 修改 `organized_edge_detection.hpp`，抽出 production helper 和 `__RVV10__` 分流 | 非 RVV 构建走原标量；RVV 构建满足 gate 时命中 RVV，否则 fallback。 |
| PI3 production direct tests | 扩展 `src/test_organized_edge_detection.cpp`，用真实 `OrganizedEdgeBase::compute()` 对比 Std/RVV 输出 | QEMU Std/RVV 均通过，覆盖 finite、NaN boundary、edge type gate。 |
| PI4 evidence rerun | `run_test_compare`、production asm、board repeated、Evidence Doctor、registry | 无 correctness failure；asm 归属到 production helper；board bucket 为 positive 才采纳。 |
| PI5 decision | 更新 phase result、matrix、evaluation、roadmap、队列表和 `doc-rvv/features/organized_edge_detection-RVV.zh.md` | 若 production board positive，写 adopted production behavior；否则不采纳。 |

## Fallback Matrix

| gate | RVV 行为 | fallback |
| --- | --- | --- |
| `__RVV10__` 未定义 | 不编译 RVV helper | 原标量 helper。 |
| `PointT` 不满足 z-field 单 float AoS layout | 不进入 RVV | 原标量 helper。 |
| `PointLT` 不是 `pcl::Label` | 不进入 RVV | 原标量 helper。 |
| 宽或高小于 3 | 无内部像素 | 原语义空操作。 |
| edge type 不含 depth / NaN boundary bits | 不进入 helper 主体 | 保持原标量早退语义。 |
| lane 有 invalid neighbor | 当前 vector chunk 先写全有限结果，再逐 lane 标量修正 | 保持 search-neighbor 和 NaN boundary 语义。 |

## 板卡复跑预算和决策桶

本阶段使用 5-run repeated board budget（重复板卡预算）。若 public/production bench 的各 case median 和 mean
均大于 `1.05x` 且 checksum match、Evidence Doctor 无 Error，则判为 `positive` 并采纳。若落在
`1.00x-1.05x`，按 weak-positive 处理，只有实现足够小且无 Warning 才可采纳；若低于 `1.00x` 或 checksum
不一致，则不采纳。预算用完后 bucket 稳定即可关闭，不无限复跑。

## 文档更新清单

- `doc/phases/010-production-depth-label-probe/result.zh.md`
- `doc/phases/optimization-matrix.zh.md`
- `doc/organized_edge_detection-evaluation.zh.md`
- `doc/optimization-roadmap.zh.md`
- `README.zh.md`
- `doc-rvv/features/organized_edge_detection-RVV.zh.md`（仅 production 证据 positive 且采纳时创建）
- `doc-rvv/library-screening/features/features-function-evaluation-queue.zh.md`

## 继续 / 停止条件

继续到 PI2-PI5 的前提是 RED asm gate 正常失败且 production patch 可保持公开 API 不变。停止条件包括：
production correctness 失败且无法局部修复、asm 无法归属到 production helper、板卡不可达、board bucket
negative / unstable、Evidence Doctor Error 无法解释，或需要扩大到 RGB/normal Canny、其它 public API、
其它 topic。
