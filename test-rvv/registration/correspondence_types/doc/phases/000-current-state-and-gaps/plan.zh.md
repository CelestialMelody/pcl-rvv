# Phase 000 计划：current-state-and-gaps

## 阶段意图和边界

本阶段启动 `registration/include/pcl/registration/impl/correspondence_types.hpp` 的 RVV 评估，先回答三个问题：

1. `getQueryIndices` 和 `getMatchIndices` 的固定字段抽取是否可以作为低风险 RVV candidate（候选实现）验证。
2. `getCorDistMeanStd` 的距离均值 / 标准差归约是否存在值得继续的 RVV 方向，以及它的数值语义是否允许改变累加树。
3. 该 topic 是否有足够证据进入 production integration loop（生产接入闭环）。

本阶段只修改配置解析出的 topic 测试资产和文档，不修改 production（生产源码）。生产接入需要后续阶段补齐 production direct（真实生产路径证据）、fallback（回退路径）、反汇编归属和板卡性能证据。

## S0 偏好冻结

```yaml
preferences_loaded:
  defaults: loaded
  local_override: absent
  prompt_override: "用户指定 repo、RVV worker 和 topic 文件"
frozen_policies:
  comment_policy:
    test_rvv: detailed_zh
    diagnostic: detailed_zh
    prototype: detailed_zh
    production: concise_boundary_only
  documentation_policy:
    closeout_current_state_first: true
    require_numeric_example: true
    forbid_conversation_process_phrasing: true
  evidence_policy:
    default_policy: summary-only
    raw_logs: do_not_submit_by_default
  work_log_policy:
    save_topic_artifacts_only: true
    current_handoff: local_only_unless_user_authorizes
  commit_policy: no_commit_without_user_request
  agent_asset_feedback_policy: report-only
```

## 解析出的产物路径

| artifact key | template key | resolved path | publication class |
| --- | --- | --- | --- |
| topic test dir | `artifact_layout.topic_test_dir_template` | `test-rvv/registration/correspondence_types` | topic test asset |
| evaluation | `artifact_layout.evaluation_doc_template` | `test-rvv/registration/correspondence_types/doc/correspondence_types-evaluation.zh.md` | final_topic_docs |
| topic doc | `artifact_layout.topic_doc_template` | `doc-rvv/registration/correspondence_types-RVV.zh.md` | production-only topic doc；no-production 时 not_applicable |
| phase plan | `artifact_layout.phase_plan_template` | `test-rvv/registration/correspondence_types/doc/phases/000-current-state-and-gaps/plan.zh.md` | phase_docs |
| phase result | `artifact_layout.phase_result_template` | `test-rvv/registration/correspondence_types/doc/phases/000-current-state-and-gaps/result.zh.md` | phase_docs |
| optimization roadmap | `artifact_layout.optimization_roadmap_template` | `test-rvv/registration/correspondence_types/doc/optimization-roadmap.zh.md` | phase_docs |
| evidence registry | `artifact_layout.evidence_registry_template` | `test-rvv/registration/correspondence_types/log/evidence_registry.json` | evidence_summary |

## 当前状态清单

| 对象 | 状态 | 证据 |
| --- | --- | --- |
| production helper | 三个 inline helper 尚无 RVV 分支 | `registration/include/pcl/registration/impl/correspondence_types.hpp` |
| 上游声明 | 公开头只声明三个 helper 并 include impl | `registration/include/pcl/registration/correspondence_types.h` |
| `Correspondence` 布局 | `index_query`、`index_match`、`distance/weight` 三个 32-bit 字段，容器使用 Eigen aligned allocator | `common/include/pcl/correspondence.h` |
| 筛选队列 | `correspondence_types` 为 registration 建议优化队列第 7 项，状态为待评估 | `doc-rvv/library-screening/registration/registration-function-evaluation-queue.zh.md` |
| topic-local 测试资产 | 不存在，需本阶段创建 | `test-rvv/registration/correspondence_types/` |

## 标量路径与候选族

| helper | 标量语义 | 初始 RVV 候选 | 风险 |
| --- | --- | --- | --- |
| `getQueryIndices` | 按输入顺序把 `index_query` 写入 `pcl::Indices` | 对 `Correspondence` AoS（结构数组）字段做 `vlse32` 跨步加载，再连续 `vse32` 写出 | 需要确认 `pcl::index_t` 为 32-bit，字段 offset 与 `sizeof(Correspondence)` 固定 |
| `getMatchIndices` | 按输入顺序把 `index_match` 写入 `pcl::Indices` | 同上，offset 换成 `index_match` | sentinel（哨兵值）`-1` 必须原样保留 |
| `getCorDistMeanStd` | 对 `distance` 做 `double sum`；平方项源码为 `float * float` 后转 `double`；样本方差用 `n - 1` | 先做 diagnostic（诊断）版本，比较保持 float-square 语义的 RVV 规约和标量参考 | `n == 1` 的既有行为会除以 0；RVV reduction（向量规约）会改变累加树，不能无预算接 production |

## 优化矩阵

| candidate family | row source policy | point type / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| strided-index-extract | correspondences（对应关系数组顺序扫描） | `pcl::Correspondence` 12-byte AoS，`pcl::index_t` 32-bit | `getQueryIndices` / `getMatchIndices` test-only candidate | query/match 顺序、重复、负 sentinel、小规模 fallback | `index-extract` QEMU smoke；board repeated 待后续 | blocked until board | `dump_bench_rvv` 需出现 `vlse32` / `vse32` 且归属当前 bench helper | manual doctor until manifest exists | planned | 本阶段实现候选、跑 QEMU correctness 和 asm smoke |
| distance-stats-reduction | correspondences 顺序扫描 | `distance` float 字段，输出 double | `getCorDistMeanStd` test-only candidate | 空输入、`n == 1`、一般样本、高动态范围样本、NaN/Inf 传播 | `distance-stats` QEMU smoke；board repeated 待后续 | blocked until board | `vfred*` 或 `vfw*` 是否出现需归属 | manual doctor until manifest exists | planned | 本阶段先用宽容误差预算验证，不接 production |
| production-dispatch | production public helpers | 同上 | 真实 `correspondence_types.hpp` inline helper | production direct tests、fallback、non-RVV build | production bench | not_started | production symbol/inline attribution pending | pending | deferred | 需要首阶段证据正向后进入 PI1 |

## 实现和测试动作

| action | 产物 | 预期证据 | 完成判据 |
| --- | --- | --- | --- |
| A1 建立 test support 聚合入口 | `include/correspondence_types.h`、`include/impl/correspondence_types_candidates.hpp` | 代码注释说明测试证据边界 | std/RVV 两种构建均可编译 |
| A2 建立 correctness tests | `src/test_correspondence_types.cpp` | gtest 覆盖空输入、单元素、重复索引、sentinel、顺序保持、距离统计误差预算 | `make run_test_compare` 通过 |
| A3 建立 bench smoke | `src/bench_correspondence_types.cpp`、`Makefile` | 可解析 benchmark 输出，QEMU 仅作为 smoke | `make run_bench_compare` 生成 compare summary；不写性能结论 |
| A4 反汇编 smoke | `make dump_bench_rvv` | `build/asm/riscv/bench_correspondence_types_rvv.asm` 中存在目标 RVV 指令 | 只能证明路径 / 指令存在，热点归属仍需人工说明 |
| A5 写阶段结果和文档 | `result.zh.md`、evaluation、topic doc | 分层说明 correctness、QEMU、asm、board 缺口和生产判断 | Handoff 能恢复下一阶段 |

## Evidence Doctor 和 registry 规则

本阶段没有 topic-local manifest wrapper。Evidence Doctor（证据体检）先按规则人工检查：

- QEMU bench summary 若存在 timing，只标为 `qemu_smoke_only`，不能进入性能结论。
- checksum 必须 std/RVV 一致，否则禁止后续性能判断。
- asm attribution（反汇编归属）只到 bench helper 二进制和指令级；production 符号归属本阶段不适用。
- `log/evidence_registry.json` 尚未创建，Handoff 写 `not_available`，下一阶段若要提交 evidence summary 再接入 registry target。

## 板卡复跑预算和决策桶

本阶段不运行 board benchmark（板卡性能测试）。后续若进入 `010-board-diagnostic-and-production-decision`，默认预算：

| 项 | 默认值 |
| --- | --- |
| runs | 5 |
| warm-up | 5 iterations |
| per-run iterations | 20 |
| decision bucket | `positive` > 1.15x；`weak_positive` 1.03x-1.15x；`neutral` 0.97x-1.03x；`negative` < 0.97x；跨桶摇摆为 `unstable` |
| rerun budget | Evidence Doctor warning 或跨桶时最多补 1 轮同边界复跑 |

## 继续 / 停止条件

继续到下一阶段的条件：

- QEMU correctness 通过；
- index extraction 或 distance stats 至少一个候选在 asm 中出现目标 RVV 指令；
- bench smoke 可生成 checksum 和 compare summary。

停止或降级条件：

- correctness 不一致；
- RVV 构建未能编译；
- asm 无法证明候选指令存在；
- 继续需要修改 production；本阶段未授权 production patch。

## 文档更新清单

- 更新 `doc/correspondence_types-evaluation.zh.md` 的 S2 函数级评估、Traceability Map、测试计划和初始生产判断。
- 创建 `doc/optimization-roadmap.zh.md`，记录后续 board diagnostic、PI1、production dispatch、distance reduction 数值预算等候选。
- 不为仅诊断或 no-production 结论创建 `doc-rvv/registration/correspondence_types-RVV.zh.md`；诊断边界先写入 evaluation 和 phase docs，只有真实 production 接入后再创建长期主题入口。
