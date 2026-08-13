# Phase 040 Result: Doc Suite Parity

## 执行范围

本阶段按 plan 处理 topic-local doc suite parity（主题本地文档套件对齐）。实际执行范围只覆盖当前 topic
的 README、topic-local docs、phase recovery 文档和长期 topic doc 的导航引用：

- 新增 `README.zh.md`。
- 新增 `doc/testing-overview.zh.md`、`doc/correctness-tests.zh.md`、
  `doc/benchmark-and-evidence.zh.md`、`doc/optimization-evidence.zh.md` 和
  `doc/test-support-code-map.zh.md`。
- 更新 phase README、optimization matrix、roadmap、evaluation 和长期 topic doc。
- 运行 registry freshness check、`git diff --check` 和 std/RVV QEMU correctness 临时日志验证。

本阶段没有修改 production header、RVV hot path、dispatch gate、fallback gate、测试语义、bench case、
case filter、board target 或 EvidenceDecision。

## 计划动作完成矩阵

| id | 状态 | 命令 / 证据 | 结论 |
| --- | --- | --- | --- |
| D0 写 phase plan 并降级旧 ready_for_review | done | `doc/phases/040-doc-suite-parity/plan.zh.md` | plan 先于本阶段 doc suite 修改存在；Phase 030 的旧停止位因 doc suite 缺口被标为 stale stop decision。 |
| D1 新增 README 和 doc suite | done | `README.zh.md`、`doc/testing-overview.zh.md`、`doc/correctness-tests.zh.md`、`doc/benchmark-and-evidence.zh.md`、`doc/optimization-evidence.zh.md`、`doc/test-support-code-map.zh.md` | 测试类型、gtest 语义、bench label、证据白名单和代码地图有稳定主归属。 |
| D2 同步恢复入口和矩阵 | done | `doc/phases/README.zh.md`、`doc/phases/optimization-matrix.zh.md`、`doc/optimization-roadmap.zh.md`、evaluation、topic doc | Phase 040 状态、doc suite 导航和 remaining deferred items 已同步。 |
| D3 Evidence freshness 检查 | done | `python3 test-rvv/script/evidence_registry.py check ... --require-doc-ref --fail-on any` | registry check fresh；新增文档引用没有制造 doc_ref_missing，也没有覆盖登记 evidence。 |
| D4 验证 | done | `git diff --check`；`run_test_std` / `run_test_rvv` 写到 `/tmp/teptpl_phase040_run_test_*.log` | diff check 干净；std/RVV QEMU correctness 均 40/40 通过。 |
| D5 回填 phase result | done | 本文件。 | 阶段闭合，production candidate 不变。 |

## 新增文档套件

| 文档 | 主职责 | 不承担什么 |
| --- | --- | --- |
| `README.zh.md` | 目录导航、常用命令、当前可提交证据和不覆盖范围。 | 不复制每个 TEST 或 bench label 的长解释。 |
| `doc/testing-overview.zh.md` | 测试类型、运行入口、覆盖矩阵和 evidence policy。 | 不写 production 实现细节。 |
| `doc/correctness-tests.zh.md` | 每个 gtest 的输入、断言、证明范围和不能外推的边界。 | 不写性能结论。 |
| `doc/benchmark-and-evidence.zh.md` | bench label 字典、case-filter、checksum、asm、board summary、registry 和日志提交边界。 | 不把 QEMU timing 写成性能结论。 |
| `doc/optimization-evidence.zh.md` | candidate family 到代码、target、证据和决策的索引。 | 不替代 roadmap 的后续搜索空间。 |
| `doc/test-support-code-map.zh.md` | 聚合入口、内部头、test/bench 源和 production helper 的调用关系。 | 不把 test-support helper 说成 production API。 |

## Ready For Review Validity Check

检查对象：

- 最近 phase result：Phase 030 和本 Phase 040。
- phase README、optimization roadmap、optimization matrix。
- mature sibling parity 表。
- 当前 shape scan：`src/test_teptpl.cpp`、`src/bench_teptpl.cpp`、`include/teptpl.h`、`test_support/*.hpp`、
  README 和 topic-local doc suite。
- legacy compatibility decision 和 evidence registry 状态。

结果：

| area | 状态 | 说明 |
| --- | --- | --- |
| topic-local doc suite | adopted | 本阶段补齐 README、testing overview、correctness tests、benchmark/evidence、optimization evidence 和 code map。 |
| evaluation 主路径 | adopted | 主归属仍是 `doc/transformation_estimation_point_to_plane_lls-evaluation.zh.md`；根目录 pointer 已在 Phase 030 删除。 |
| doc-rvv 分工 | adopted | 长期文档只补导航引用，测试/bench 细节转到 topic-local docs。 |
| legacy pointer / alias | adopted / deleted | 旧 evaluation pointer 和旧 test-support alias 已删除。 |
| evidence registry | fresh | registry check fresh。 |
| source split | turn_stop_deferred | `src/test_teptpl.cpp` 和 `src/bench_teptpl.cpp` 仍长；但本阶段 doc suite 和 code map 已降低 reviewer 定位成本。继续拆分会产生大型 C++ 搬迁 diff，需独立 phase 和 review focus。 |
| production helper shape | turn_stop_deferred | 会触碰 hot path，需要独立 correctness / asm / board 风险判断。 |
| row-source / point-type / `Scalar` expansion | turn_stop_deferred | 会扩大证据范围，不能继承 full-cloud production candidate。 |

`next_phase_default=ready_for_review_validity_checked`。若 reviewer 仍要求继续当前 topic，默认恢复
`050-test-source-split`；否则当前 topic 可进入 review。

## Evidence Doctor、Registry 和 Freshness

本阶段没有生成新的 benchmark、board summary、checksum summary 或 asm attribution（反汇编归属），因此没有运行
JSON manifest 形式 Evidence Doctor（证据体检）。人工 doctor 结论：

- 本阶段是 documentation-only，不产生新的 performance evidence。
- 既有 board summary 仍是 summary-only metadata incomplete（摘要证据元数据不完整），不能替代完整 JSON manifest。
- QEMU correctness 临时日志只证明本轮构建/测试通过；不更新当前 registry digest，也不作为可提交 evidence。

registry check 命令：

```text
python3 test-rvv/script/evidence_registry.py check \
  --registry test-rvv/registration/transformation_estimation_point_to_plane_lls/log/evidence_registry.json \
  --scan-glob 'test-rvv/registration/transformation_estimation_point_to_plane_lls/output/board/**/*.md' \
  --scan-glob 'test-rvv/registration/transformation_estimation_point_to_plane_lls/log/qemu/*.log' \
  --doc doc-rvv/registration/transformation_estimation_point_to_plane_lls-RVV.zh.md \
  --doc test-rvv/registration/transformation_estimation_point_to_plane_lls/README.zh.md \
  --doc test-rvv/registration/transformation_estimation_point_to_plane_lls/doc/transformation_estimation_point_to_plane_lls-evaluation.zh.md \
  --doc test-rvv/registration/transformation_estimation_point_to_plane_lls/doc/benchmark-and-evidence.zh.md \
  --doc test-rvv/registration/transformation_estimation_point_to_plane_lls/doc/phases/README.zh.md \
  --require-doc-ref \
  --fail-on any
```

结果为 `evidence registry check: fresh`。

## 验证结果

| 命令 | 结果 | 边界 |
| --- | --- | --- |
| `git diff --check -- test-rvv/registration/transformation_estimation_point_to_plane_lls doc-rvv/registration/transformation_estimation_point_to_plane_lls-RVV.zh.md registration/include/pcl/registration/impl/transformation_estimation_point_to_plane_lls.hpp` | pass | 无 whitespace error。 |
| `make -C test-rvv/registration/transformation_estimation_point_to_plane_lls run_test_std TEST_STD_OUTPUT_FILE=/tmp/teptpl_phase040_run_test_std.log` | pass，40/40 tests | QEMU correctness；临时日志不进入提交边界。 |
| `make -C test-rvv/registration/transformation_estimation_point_to_plane_lls run_test_rvv TEST_RVV_OUTPUT_FILE=/tmp/teptpl_phase040_run_test_rvv.log` | pass，40/40 tests | QEMU correctness；临时日志不进入提交边界。 |
| registry check | pass / fresh | 没有发现未登记覆盖或 doc_ref_missing。 |

本阶段没有运行 QEMU bench、反汇编或板卡 benchmark，因为文档套件补齐不改变 RVV hot path、bench case、
case filter、dispatch gate 或 fallback gate。

## Dirty Isolation

当前可审查 topic diff 包含：

```text
doc-rvv/registration/transformation_estimation_point_to_plane_lls-RVV.zh.md
registration/include/pcl/registration/impl/transformation_estimation_point_to_plane_lls.hpp
test-rvv/registration/transformation_estimation_point_to_plane_lls/
```

worktree 中仍存在 agent asset、weighted topic、module second-pass、`test-rvv/.gitignore` 和 weighted board
evidence dirty。它们属于 separate review / do not stage for this phase。本阶段没有读取 weighted 证据数字作为当前
topic 结论，只把 weighted sibling 的文档/目录结构作为 maturity quality bar。

## Continue / Stop Decision

当前 phase 完成，`unblocked_next_actions=none inside Phase 040`。停止条件命中：

- D0-D5 已闭合。
- topic-local doc suite parity 已 adopted。
- registry check fresh。
- std/RVV QEMU correctness 均 40/40 通过。
- production candidate、bench case、dispatch gate 和 fallback gate 没有变化。
- 剩余方向要么是大型 C++ 搬迁 diff（test source split），要么会触碰 production hot path，或扩大 row source / 点型 / `Scalar` 证据范围。

`continue_stop_decision=stop_for_review`。
`next_phase_default=ready_for_review_validity_checked`。
若 reviewer 要求继续，推荐默认下一 phase 是 `050-test-source-split`，并先写独立 plan。
