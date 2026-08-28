# Normal-plane 测试支撑结构 Phase Plan

## 阶段意图和边界

本阶段只整理 `test-rvv/sample_consensus/plane_models` 的 topic-local 测试支撑结构和文档套件，不改变 normal-plane RVV 生产算法。phase 000 已关闭公开入口、fallback、QEMU、反汇编、板卡和 Evidence Doctor 证据；本阶段要让这些证据更容易复跑、审查和恢复。

本阶段要证明：

- 根目录测试 / bench 源码迁移到配置期望的 `src/` 后，既有 Make target 和日志路径仍可用。
- board test / bench 默认使用已部署到板卡的 PCD fixture，不再要求手工传 `BENCH_ARGS=/root/...` 或 `REMOTE_TEST_ARGS=/root/...`。
- topic-local README 和 doc-suite role 文档能把测试、bench、Evidence Doctor、production 长期文档和下一阶段动作分清。

本阶段不证明：

- 新 RVV 实现族优于已有 `f32m2` helper。
- 泛型点类型或其它 sample_consensus 模型已完成。
- raw board logs 应默认进入提交；仍采用 summary-only 策略。

## 当前状态清单

| 对象 | 当前事实 | 路径 |
| --- | --- | --- |
| phase 000 | positive closed，production patch retained。 | `test-rvv/sample_consensus/plane_models/doc/phases/000-normal-plane-current-state-and-public-entry-boundary/result.zh.md` |
| C++ test source | 根目录长文件同时覆盖 plane、normal-plane、normal-parallel-plane。 | `test-rvv/sample_consensus/plane_models/test_sample_consensus_plane_models.cpp` |
| C++ bench source | 根目录有 normal-plane helper bench 和 load-compare probe。 | `test-rvv/sample_consensus/plane_models/bench_sac_normal_plane.cpp`、`bench_sac_normal_plane_load_compare.cpp` |
| Makefile | `SRCS_*` 使用根目录文件名；board compare 会把 host `BENCH_ARGS` 传到远端。 | `test-rvv/sample_consensus/plane_models/Makefile` |
| board.mk | board 侧已定义 `/root/pcl-test/sample_consensus/plane_models/pcd/sac_plane_test.pcd`，但 host rule 会覆盖。 | `test-rvv/sample_consensus/plane_models/board.mk` |
| doc suite | 已有 evaluation、roadmap、phase index、optimization matrix；缺 topic README、testing overview、correctness tests、benchmark/evidence、optimization evidence、test-support code map。 | `test-rvv/sample_consensus/plane_models/doc/` |

## 假设与候选族

| candidate family | 假设 | 本阶段动作 |
| --- | --- | --- |
| source layout split | 把根目录 C++ 文件移到 `src/` 可以降低 topic 根目录噪声，不改变编译输入。 | 移动三份 `.cpp`，更新 `SRCS_TEST`、`SRCS_BENCH`、`SRCS_BENCH_LOAD`。 |
| board fixture default | host Makefile 可以用 target-specific variable（目标专属变量）给 board target 设置远端 PCD 路径。 | 设置 `run_board_test`、`run_board_bench_compare`、`board_smoke` 的 board-side args。 |
| target granularity aliases | phase 000 的 public/fallback/helper tests 值得有短 alias，避免 reviewer 手写 filter。 | 增加 `run_normal_plane_public_tests` 和 board 对应 alias。 |
| doc suite minimal split | 当前 topic 命中复杂 topic 条件，但无需把 C++ helper 再拆成 `include/impl`，因为还没有独立 helper header。 | 新增 role 文档并在 code map 中把 `include/impl` 记为 not_applicable with evidence。 |

## 优化矩阵

| candidate family | scope | correctness target | board target | Evidence Doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- |
| source layout split | `src/` 下三份 topic-owned `.cpp` | `run_test_rvv`、`run_test_compare` | not required if source-only path verified locally | not_applicable | planned | 移动源码并更新 Makefile |
| board fixture default | board test / bench args | `run_board_test` | `run_board_bench_compare` | phase 000 manifest can be regenerated later | planned | target-specific variable 修正 |
| public/fallback aliases | normal-plane phase 000 gtest filters | alias target green | board alias green if budget允许 | not_applicable | planned | 增加 Make target |
| doc suite role split | topic-local docs | doc scan / artifact tracking | not_applicable | not_applicable | planned | README + role docs |

## 实现和测试动作

| action | 产物 | 命令 / 证据 | 完成判据 |
| --- | --- | --- | --- |
| A1 phase plan | 本文件 | `git status --short --untracked-files=all -- test-rvv/sample_consensus/plane_models doc-rvv/sample_consensus` | plan 先于结构编辑存在 |
| A2 source move | `src/test_sample_consensus_plane_models.cpp`、`src/bench_sac_normal_plane.cpp`、`src/bench_sac_normal_plane_load_compare.cpp`、Makefile | `make -C test-rvv/sample_consensus/plane_models run_test_rvv` | RVV test 仍通过 |
| A3 board args | Makefile target-specific vars | `make -C test-rvv/sample_consensus/plane_models run_board_bench_compare fetch_board_logs`、`run_board_test fetch_board_logs` | 不再需要命令行 PCD override |
| A4 alias targets | Makefile aliases | `make -C test-rvv/sample_consensus/plane_models run_normal_plane_public_tests` | public/fallback/helper filter 通过 |
| A5 doc suite | README 与 `doc/*.zh.md` role docs | `rg` / `git status --short --untracked-files=all -- test-rvv/sample_consensus/plane_models` | README 能指向每个 role，untracked artifacts 明确在 topic boundary 内 |
| A6 closeout | phase result、matrix、roadmap、evaluation | `git diff --check -- <topic paths>` | phase result 给出 doc_suite_role_inventory 和下一步 |

## Evidence Doctor 和 registry 规则

本阶段不新增性能结论，因此不要求重新生成 Evidence Doctor。若 A3 重新跑 board compare 并覆盖 phase 000 `log/board`，必须确认 phase 000 manifest 与 analyzer summary 仍一致；若数值 bucket 仍 positive，可只在 phase 010 result 中引用 rerun。topic 仍缺自动 registry，记录为后续 `020-evidence-registry-target-alias` 候选，除非本阶段时间内可无风险补齐。

## 板卡复跑预算和决策桶

板卡可用。本阶段最多跑 1 轮 board bench compare 和 1 轮 board test，用于验证 Makefile 参数边界；若 board 因网络或设备问题不可达，阶段不因此回滚 source/doc 结构，但把 board args 验证写为 `blocked`。

## diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | 本阶段是 test-support / doc-suite 结构证据，不新增 production performance 证据。 |
| A/B boundary | board compare 如重跑，仍是 protected helper hot-path bench；公开入口由 unit test 覆盖。 |
| 当前决策问题 | harness correctness / doc-suite reviewability，不是 RVV-vs-scalar 取舍。 |
| diagnostic 是否可外推到 production | 不外推；phase 000 已承担 production patch retained 判断。 |
| comparison-boundary / baseline mismatch 风险 | 主要风险是 host PCD 路径误传到 board；本阶段直接修正该边界。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 不适用；本阶段不做新 production probe。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 不需要；不做新实现族选择。 |

## 继续 / 停止条件

继续条件：source layout、board args、alias target、doc suite 任一项仍为 `phase_deferred + unblocked`。停止条件：移动后测试失败且三轮内不能定位、board 不可达、dirty isolation 与用户改动冲突，或需要扩大到其它 sample_consensus topic。

默认下一 phase：若本阶段关闭，进入 `020-normal-plane-evidence-registry-target-alias`，把 phase-local manifest 生成、registry、sanitized summary 和 Make alias 收敛为可复用证据入口。

## 文档更新清单

- `test-rvv/sample_consensus/plane_models/README.zh.md`
- `test-rvv/sample_consensus/plane_models/doc/testing-overview.zh.md`
- `test-rvv/sample_consensus/plane_models/doc/correctness-tests.zh.md`
- `test-rvv/sample_consensus/plane_models/doc/benchmark-and-evidence.zh.md`
- `test-rvv/sample_consensus/plane_models/doc/optimization-evidence.zh.md`
- `test-rvv/sample_consensus/plane_models/doc/test-support-code-map.zh.md`
- `test-rvv/sample_consensus/plane_models/doc/phases/010-normal-plane-test-support-structure/result.zh.md`
