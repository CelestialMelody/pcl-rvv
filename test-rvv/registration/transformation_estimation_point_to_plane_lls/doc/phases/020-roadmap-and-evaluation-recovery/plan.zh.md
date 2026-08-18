# Phase 020 Plan: Roadmap And Evaluation Recovery

## 阶段意图和边界

本阶段补齐 Phase 010 后仍影响恢复和审查的文档结构缺口。当前 topic 已经有
`doc/phases/` 和 optimization matrix（优化矩阵），但缺少按当前配置解析出的
topic-level optimization roadmap（主题级优化路线图）。evaluation（函数级评估）
仍在 topic 根目录，而 `.agents/config/defaults.yaml` 的
`artifact_layout.evaluation_doc_template` 已解析到 `doc/` 子目录。这个不一致会让下一轮
短 prompt worker 和 reviewer 在恢复时读取两套入口。

本阶段只做恢复与文档归属整理：

- 新增 `doc/optimization-roadmap.zh.md`，把候选搜索空间、暂缓路线和下一阶段恢复条件放到
  roadmap 主归属。
- 将 evaluation 主文档迁到 `doc/transformation_estimation_point_to_plane_lls-evaluation.zh.md`，
  根目录旧路径保留为 legacy pointer（旧路径指针），降低旧引用风险。
- 更新 phase README、optimization matrix、topic-local 文档引用和必要筛选 / 主题文档引用。

本阶段不做：

- 不修改 `registration/include/pcl/registration/impl/transformation_estimation_point_to_plane_lls.hpp`。
- 不修改 `src/test_teptpl.cpp`、`src/bench_teptpl.cpp` 或 bench case 逻辑。
- 不运行 QEMU timing、板卡 bench 或刷新 performance（性能）结论。
- 不接入 evidence registry（证据登记表），只记录它仍是未阻塞后续项。
- 不修改 `.agents/` agent asset；当前 `.agents/` dirty 继续作为 separate review 边界。

## S0 和 dirty isolation

| 项 | 当前状态 |
| --- | --- |
| `preferences_loaded` | 已读取 `AGENTS.md`、`.agents/config/defaults.yaml`、`.agents/knowledge/pcl-rvv-knowledge-map.md`、`rvv-workflow`、`rvv-test`、`rvv-documentation` 相关规则；`.agents/local/user-preferences.yaml` 不存在；当前 prompt 要求恢复 phase loop、检查 git status 和 dirty isolation。 |
| 注释策略 | test-rvv / diagnostic / prototype 详细中文；production 注释只解释维护边界、fallback、dispatch、数值风险和数据布局。 |
| 文档策略 | closeout 当前状态优先；长期文档不写对话流程；英文术语首次出现带中文解释。 |
| 证据策略 | summary-only；raw logs 不默认提交；QEMU 只用于 correctness、路径和日志形状。 |
| dirty isolation | 当前 dirty 分为 agent asset、topic 源码 / 测试 / 文档、模块筛选文档三类。本阶段只允许修改当前 topic 的文档导航、phase 文档、evaluation 路径和必要引用；`.agents/` dirty 不继续修改。 |

当前允许路径：

```text
test-rvv/registration/transformation_estimation_point_to_plane_lls/
doc-rvv/registration/transformation_estimation_point_to_plane_lls-RVV.zh.md
doc-rvv/library-screening/registration/registration-function-evaluation-queue.zh.md
```

## 当前状态清单

| 领域 | 当前事实 | 路径 |
| --- | --- | --- |
| production candidate | full-cloud f32 AoS layout-gated `Scalar=float` fused-formula block dispatch；三类代表点型 5-run 板卡正向。 | `doc-rvv/registration/transformation_estimation_point_to_plane_lls-RVV.zh.md` |
| Phase 000 | reference / production-detail boundary cleanup 已完成；std/RVV QEMU correctness 40/40 通过。 | `doc/phases/000-current-state-and-gaps/result.zh.md` |
| Phase 010 | test / bench 源码迁到 `src/`，新增 `include/teptpl.h`，std/RVV QEMU correctness 40/40 通过。 | `doc/phases/010-test-harness-layout-audit/result.zh.md` |
| roadmap | 缺失。 | 应为 `doc/optimization-roadmap.zh.md` |
| evaluation | 主文档仍在 topic 根目录。 | 应迁到 `doc/transformation_estimation_point_to_plane_lls-evaluation.zh.md` |
| evidence registry | topic 尚无 `log/evidence_registry.json`。 | 当前 phase 只记录，不接入。 |

## 假设与候选族

| 假设 | 本阶段验证方式 |
| --- | --- |
| roadmap 和 evaluation 路径整理不会改变源码或测试行为。 | `git diff --check`；必要时运行 std/RVV correctness。 |
| 根目录旧 evaluation 保留短指针能降低旧引用风险。 | `rg` 检查文档引用，更新 phase README 和长期文档中的主路径。 |
| roadmap 应记录仍可尝试但不自动推进的路线。 | 将 helper size review、test source split、evidence registry、更多点型、`Scalar=double`、indexed / correspondences profile 写入 roadmap，并标出恢复条件。 |

## 本阶段优化矩阵

本阶段新增 `roadmap / evaluation recovery` 行，见 `../optimization-matrix.zh.md`。生产候选状态不变。

## 实现和测试动作

| id | 动作 | 产物 / 命令 | 完成判据 |
| --- | --- | --- | --- |
| R0 | 写本 phase plan 并冻结范围。 | 本文件。 | plan 先于 Phase 020 文档迁移存在。 |
| R1 | 新增 roadmap。 | `doc/optimization-roadmap.zh.md`。 | 包含当前边界、候选搜索空间、阶段反思新增路线和暂缓 / 拒绝路线。 |
| R2 | 迁移 evaluation 主路径并保留旧入口。 | `doc/transformation_estimation_point_to_plane_lls-evaluation.zh.md`；根目录 legacy pointer。 | 新路径承载完整评估；旧路径只指向新主归属。 |
| R3 | 同步文档导航和引用。 | phase README、optimization matrix、主题文档、必要筛选状态表。 | 文档主归属、Traceability Map 和恢复入口指向新 evaluation / roadmap。 |
| R4 | 验证文档迁移。 | `git diff --check`；`rg` 引用检查；必要时 `make ... run_test_std/run_test_rvv`。 | diff check 干净；没有旧主路径仍被写成唯一主归属。 |
| R5 | 回填 phase result。 | `result.zh.md`。 | 每项动作有 `done / partial / deferred / blocked`、证据路径和 continue / stop decision。 |

## Evidence Doctor 和 registry 规则

本阶段不生成新的 benchmark、board summary、checksum summary 或 asm attribution，因此不运行
JSON manifest 形式 Evidence Doctor。`evidence_registry_status=not_available`；本阶段只做
文档恢复，不覆盖 generated logs。

## 板卡复跑预算和决策桶

本阶段板卡复跑预算为 `0`。理由是 roadmap / evaluation 路径整理不改变 RVV hot path、bench case、
case filter、dispatch gate 或 fallback gate。既有 production-dispatch decision bucket 保持
`positive` within current production candidate boundary。

## 继续 / 停止条件

继续条件：

- 引用迁移发现文档互相矛盾，可在当前 topic 文档边界内修复。
- `git diff --check` 失败，且可通过文档格式修复。

停止条件：

- R0-R5 闭合，且剩余动作属于后续独立 phase：拆分大型 test/bench 源文件、迁移 `test_support/`
  到 `include/impl/`、接入 evidence registry、压缩 production RVV helper 或扩大 row source。
- 继续需要修改 production hot path、bench case、dispatch gate 或运行板卡。

默认下一阶段：

- 若本阶段通过，`next_phase_default=ready_for_review`。
- 若 reviewer 要求继续当前 topic，优先从 roadmap 中选择一个高优先级未阻塞项，例如
  `030-evidence-registry-adoption` 或 `030-helper-shape-review`。

## 文档更新清单

| 文档 | 本阶段动作 |
| --- | --- |
| `doc/optimization-roadmap.zh.md` | 新建主题级路线图。 |
| `doc/transformation_estimation_point_to_plane_lls-evaluation.zh.md` | 作为 evaluation 主归属。 |
| `transformation_estimation_point_to_plane_lls-evaluation.zh.md` | 改为 legacy pointer。 |
| `doc/phases/README.zh.md` | 更新当前恢复入口、evaluation 主路径和 roadmap 状态。 |
| `doc/phases/optimization-matrix.zh.md` | 增加 roadmap / evaluation recovery 行。 |
| `doc/phases/020-roadmap-and-evaluation-recovery/result.zh.md` | 阶段结束时新建并回填事实。 |
| `doc-rvv/registration/transformation_estimation_point_to_plane_lls-RVV.zh.md` | 补充 roadmap / evaluation 新路径引用。 |
