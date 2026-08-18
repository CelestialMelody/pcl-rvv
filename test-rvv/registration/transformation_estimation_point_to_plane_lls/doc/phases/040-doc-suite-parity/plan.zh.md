# Phase 040 Plan: Doc Suite Parity

## 阶段意图和边界

本阶段恢复 Phase 030 后的 `ready_for_review` 停止位，并按当前 phase loop 规则重新做
ready-for-review validity check（评审就绪合法性检查）。检查结果是：当前 topic 已有
`doc/optimization-roadmap.zh.md`、phase 文档和 evaluation 主路径，但 topic-local doc suite
（主题本地文档套件）仍缺少 README、testing overview、correctness tests、benchmark/evidence、
optimization evidence 和 test-support code map。相邻成熟 weighted topic 已经把这些读者路径拆清，
当前 topic 复杂度也满足同类门槛：已有 production dispatch、fallback、多个 row source diagnostic、
多个候选族、repeated board summary 和 evidence registry。

本阶段采用结构经验，不迁移 weighted 的算法、证据数字或 production 结论。目标是把当前 topic 的
测试、bench、证据白名单和代码地图拆成可恢复的窄文档，让 reviewer 不必从单个 evaluation 大文档里
搜索全部语义。

本阶段做：

- 新增 topic 根目录 `README.zh.md`。
- 新增 `doc/testing-overview.zh.md`、`doc/correctness-tests.zh.md`、
  `doc/benchmark-and-evidence.zh.md`、`doc/optimization-evidence.zh.md` 和
  `doc/test-support-code-map.zh.md`。
- 更新 phase README、optimization matrix、roadmap、evaluation 和长期 topic doc 的导航状态。
- 运行文档 / freshness 验证；若重跑 QEMU correctness，则同步 evidence registry。

本阶段不做：

- 不修改 production header、RVV hot path、dispatch gate、fallback gate、测试语义或 bench case。
- 不拆分 `src/test_teptpl.cpp` / `src/bench_teptpl.cpp`，也不把 `test_support/` 重命名到 `include/impl`。
- 不运行 QEMU timing、板卡 bench 或刷新 performance（性能）结论。
- 不修改 weighted topic 或 `.agents/` agent asset；它们属于 dirty isolation 外部现场。

## S0 和 Dirty Isolation

| 项 | 当前状态 |
| --- | --- |
| `preferences_loaded` | 已读取 `AGENTS.md`、`.agents/config/defaults.yaml`、`.agents/knowledge/pcl-rvv-knowledge-map.md`、`rvv-workflow`、short-prompt、S0、quality gates、phase loop、registration evidence、documentation ownership 和 Evidence Doctor；未发现 `.agents/local/user-preferences.yaml`；当前 prompt 覆盖为继续当前 topic 并自行判断下一步。 |
| 注释 / 文档策略 | test-rvv 和 diagnostic 中文详细说明；长期文档当前状态优先；英文术语首次出现给中文解释；不把对话过程写入长期文档。 |
| 证据策略 | summary-only；raw logs 不默认提交；QEMU 只用于 correctness 和日志形状；性能结论只来自 board summary。 |
| dirty isolation | 当前 worktree 还混有 agent asset、weighted topic、module function-evaluation queue 和 generated evidence 改动。本阶段允许路径限于当前 topic 测试 / 文档、`doc-rvv/registration/transformation_estimation_point_to_plane_lls-RVV.zh.md`，以及若验证覆盖 QEMU log 时的当前 topic registry/log digest。其它 dirty 均 separate review / do not touch。 |

当前允许路径：

```text
doc-rvv/registration/transformation_estimation_point_to_plane_lls-RVV.zh.md
test-rvv/registration/transformation_estimation_point_to_plane_lls/
```

## 当前状态清单

| 领域 | 当前事实 | 路径 |
| --- | --- | --- |
| production candidate | full-cloud f32 AoS layout-gated `Scalar=float` fused-formula block dispatch；三类代表点型 board 5-run 正向。 | `doc-rvv/registration/transformation_estimation_point_to_plane_lls-RVV.zh.md` |
| 测试源码布局 | Phase 010 已把根目录长 test / bench 源码迁到 `src/test_teptpl.cpp` 和 `src/bench_teptpl.cpp`，并新增 `include/teptpl.h` 聚合入口。 | `src/`、`include/teptpl.h` |
| 测试支撑内部布局 | `test_support/` 已按 common、RVV math、row sources、reductions、candidates 拆分；尚未迁到 `include/impl`。 | `test_support/*.hpp` |
| topic-local docs | 当前只有 evaluation、roadmap、phase README/matrix；缺 README、testing overview、correctness tests、benchmark/evidence、optimization evidence、code map。 | `doc/` |
| evidence registry | Phase 030 已接入 `log/evidence_registry.json`，登记 board summary 和 QEMU correctness logs。 | `log/evidence_registry.json` |
| mature sibling quality bar | weighted sibling 已具备 README、topic-local doc suite、`include/impl`、多 `src/test_*.cpp` 和 topic-local scripts。 | `test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted/` |

## Mature Sibling Parity Audit

| area | current shape scan | mature sibling / local quality bar | decision | blocker / evidence | next action |
| --- | --- | --- | --- | --- | --- |
| test/bench source layout | `src/test_teptpl.cpp` 1567 行，`src/bench_teptpl.cpp` 1081 行，target 名保持旧长名。 | sibling 把 gtest 拆成多份 `src/test_teptplw_*.cpp`，bench 保持薄入口。 | deferred | 本阶段目标是 doc suite；拆分会产生大搬迁 diff，需要独立 correctness / bench compile smoke。 | 保留 roadmap `040-test-source-split`。 |
| aggregator and internal helpers | `include/teptpl.h` 聚合入口；内部仍在 `test_support/*.hpp`，职责已分 common / math / row / reduction / candidates。 | sibling 使用 `include/impl` 和 test/bench 专用聚合头。 | deferred | 现有职责可审查；路径重命名收益小于本阶段文档缺口。 | 只有 test source split 后仍难审再恢复。 |
| script and bench registry | 无 topic-local wrapper；通用 bench CLI 输出 label，registry 用通用脚本登记 summary/log。 | sibling 有 topic-local scripts 和 richer board collection targets。 | rejected for this phase | 当前 topic 没有新 repeated board 或 manifest 生成；新增 wrapper 会超出 doc-suite 目标。 | 后续若新增 board collection / Evidence Doctor manifest 再建 script。 |
| topic-local docs | 缺 README、testing overview、correctness tests、benchmark/evidence、optimization evidence、code map。 | sibling 已有完整 doc suite。 | adopted | 当前缺口影响短 prompt 恢复和 reviewer 导航，无工具阻塞。 | 本阶段新增这些文档。 |
| long-term docs | `doc-rvv` 已保存 production 行为和证据链，但仍承担部分测试入口说明。 | 长期文档只保留生产行为和证据链，测试细节回 topic-local docs。 | adopted | 可用短引用迁移，不改变长期结论。 | 更新导航引用。 |
| legacy compatibility | 根目录 evaluation pointer 和旧 alias 已在 Phase 030 删除。 | 无依赖 legacy 默认删除。 | adopted | `rg` 依赖检查已在 Phase 030 通过。 | 保持 legacy-free。 |
| evidence freshness | registry fresh；新增 docs 后需要检查 doc refs 仍覆盖登记文件。 | 恢复 / 提交前 registry check。 | adopted | 若重跑 correctness 会改变 log digest，需重新 record。 | 本阶段运行 registry check，必要时 record。 |

## 假设与候选族

| 假设 | 验证方式 |
| --- | --- |
| README + 五份 topic-local 文档能关闭 doc-suite parity 缺口，而不需要同阶段拆分 C++ 源码。 | 文档归属检查、引用检查、phase result 回填。 |
| 文档-only change 不需要板卡复跑。 | `git diff` 审计证明不改 hot path、bench case、dispatch gate 或 fallback gate。 |
| 若只新增文档，registry digest 不应变化；若运行 QEMU correctness，则 registry 必须重录 QEMU log。 | `evidence_registry.py check` 和必要的 `record`。 |

## 本阶段优化矩阵

本阶段新增 `doc suite parity` 行，见 `../optimization-matrix.zh.md`。production candidate 状态不变。

## 实现和测试动作

| id | 动作 | 产物 / 命令 | 完成判据 |
| --- | --- | --- | --- |
| D0 | 写本 phase plan 并降级旧 ready_for_review。 | 本文件。 | plan 先于本阶段文档套件修改存在。 |
| D1 | 新增 README 和 doc suite。 | `README.zh.md`、`doc/testing-overview.zh.md`、`doc/correctness-tests.zh.md`、`doc/benchmark-and-evidence.zh.md`、`doc/optimization-evidence.zh.md`、`doc/test-support-code-map.zh.md`。 | 每份文档有明确主职责、证据边界和路径引用，不复制 raw log。 |
| D2 | 同步恢复入口和矩阵。 | phase README、optimization matrix、roadmap、evaluation、topic doc。 | `ready_for_review` 不再掩盖 doc-suite parity 缺口；本阶段完成后再重新判断。 |
| D3 | Evidence freshness 检查。 | `python3 test-rvv/script/evidence_registry.py check ...`。 | check fresh；如有 doc_ref_missing 或 log digest change，先更新文档或 registry。 |
| D4 | 验证。 | `git diff --check`；必要时 `make ... run_test_std` / `run_test_rvv`。 | diff check 干净；若运行 QEMU correctness，两侧通过并同步 registry。 |
| D5 | 回填 phase result。 | `result.zh.md`。 | 每项动作有 `done / partial / deferred / blocked`、证据路径和 continue / stop decision。 |

## Evidence Doctor 和 Registry 规则

本阶段不生成新的 benchmark、board summary、checksum summary 或 asm attribution，因此不运行 JSON manifest
形式 Evidence Doctor（证据体检）。对既有 board summary 只做人工 doctor 边界复核：

- summary-only metadata 不完整，不能替代完整 manifest。
- board summary 仍只支撑既有 production candidate，不因新增文档扩大范围。
- QEMU logs 只用于 correctness，不写性能结论。

registry 检查命令使用当前登记表和文档列表；新增文档若引用登记 evidence，可加入 `--doc` 输入。
如果本阶段重跑 QEMU correctness，必须用 `record` 更新 `log/evidence_registry.json` 的 digest。

## 板卡复跑预算和决策桶

本阶段板卡复跑预算为 `0`。理由是只补文档套件和引用，不改变 RVV hot path、bench case、case filter、
dispatch gate 或 fallback gate。既有 production-dispatch decision bucket 保持
`positive` within current production candidate boundary。

## 继续 / 停止条件

继续条件：

- doc suite 新增后发现 evaluation、topic doc、roadmap 或 matrix 仍互相冲突，且可在当前 topic 文档边界内修复。
- registry check 发现 doc_ref_missing、unregistered_change 或 unregistered_file，且可通过文档/registry 更新闭合。

停止条件：

- D0-D5 闭合，doc-suite parity 已采用，registry fresh，production candidate 未改变。
- 剩余动作只属于独立 phase：test source split、`test_support/` 内部 rename、production helper shape review、
  更多点型、`Scalar=double` 或 row-source production follow-up。

默认下一阶段：

- 若本阶段通过，`next_phase_default=ready_for_review`。
- 若 reviewer 仍认为 C++ 源码可读性不足，下一阶段优先 `050-test-source-split`。

## 文档更新清单

| 文档 | 本阶段动作 |
| --- | --- |
| `README.zh.md` | 新增 topic 导航、目录分工、常用命令和证据白名单。 |
| `doc/testing-overview.zh.md` | 新增测试类型、运行入口和覆盖矩阵。 |
| `doc/correctness-tests.zh.md` | 新增 gtest 名称、输入、断言和证明边界索引。 |
| `doc/benchmark-and-evidence.zh.md` | 新增 bench label、case-filter、checksum、QEMU/board/asm/registry 边界。 |
| `doc/optimization-evidence.zh.md` | 新增优化方式到代码、target、证据和结论的索引。 |
| `doc/test-support-code-map.zh.md` | 新增聚合入口、内部头、src、production helper 和证据角色地图。 |
| `doc/phases/README.zh.md` | 增加 Phase 040 恢复入口和 doc suite 状态。 |
| `doc/phases/optimization-matrix.zh.md` | 增加 doc suite parity 行。 |
| `doc/optimization-roadmap.zh.md` | 将 doc suite parity 标为本阶段处理，重排剩余候选。 |
| evaluation / topic doc | 增加 doc suite 导航引用，不复制新增文档全文。 |
