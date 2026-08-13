# Phase 050 Structure Layout Plan

## 阶段意图和边界

本阶段恢复 `roadmap_default_recovery_queue` 中仍未阻塞的结构动作：

1. `test-source-split`：拆分 `src/test_teptpl.cpp` 和 `src/bench_teptpl.cpp`，保留 Make target 名、gtest case 名、bench label 和输出合同。
2. `internal-helper-layout`：把旧 `test_support/` 中承载常规测试支撑职责的内部头迁到配置解析出的 `include/impl/`，并更新聚合入口、Makefile include graph 和文档 code map。

这是 layout-only（仅布局）阶段，不修改 production API（生产公开接口），不扩大 full-cloud production candidate 边界，不触碰 weighted topic。`registration/include/pcl/registration/impl/transformation_estimation_point_to_plane_lls.hpp` 只作为当前生产边界读证据，不在本阶段编辑。

## S0 恢复和偏好冻结

- `preferences_loaded`：defaults loaded；`.agents/local/user-preferences.yaml` absent；prompt override active。
- 注释策略：配置解析出的 test-rvv / diagnostic 资产保持详细中文注释；production 注释克制且本阶段不编辑 production。
- 文档策略：中文主导，长期文档不写对话流程话术，英文术语首次出现给中文解释。
- 证据策略：`summary-only`；raw logs 不默认提交。
- 提交策略：不 commit。
- agent asset 策略：`report-only`；本阶段只读取最新 `.agents` 资产，不修改。

## 当前 Shape Scan

| area | current shape scan | mature sibling / local quality bar | decision | blocker / evidence | next action |
| --- | --- | --- | --- | --- | --- |
| test source layout | `src/test_teptpl.cpp` 约 1567 行，混合 public semantics、full-cloud candidate、production direct、row-source diagnostic 和 fallback tests。 | weighted sibling 使用多份 `src/test_teptplw_*.cpp`；当前 topic 已采用 `teptpl` 缩写 token。 | adopted | 纯 test asset 拆分，不改 case 名。 | 拆成 public semantics、full-cloud candidates、production direct、row sources。 |
| bench source layout | `src/bench_teptpl.cpp` 约 1081 行，混合 fixture、CLI、component helper、case registry 和 `main()`。 | weighted sibling 使用薄 bench source + `include/bench_*.h` + `include/impl/*bench*`。 | adopted | 纯 bench harness 拆分，不改 label / checksum 输出。 | 增加 `include/bench_teptpl.h` 和 bench impl 头，保留薄 `src/bench_teptpl.cpp`。 |
| aggregator and internal helpers | `include/teptpl.h` 是稳定聚合入口；内部仍 include `../test_support/*.hpp`。 | 配置解析 `test_support.internal_directory=include/impl`；weighted sibling 已使用 `include/impl`。 | adopted | 当前旧 `test_support/` 承载 common、RVV math、row sources、reductions、candidates 常规职责，没有真实 blocker。 | 迁到 `include/impl/teptpl_*.hpp`，删除旧目录依赖。 |
| gtest-only helpers | shared fixture / assertion / production bridge 目前塞在 `src/test_teptpl.cpp` 顶部。 | gtest helper 不应被 bench include；按职责拆入 `include/impl`。 | adopted | 需要多 test translation units 共用。 | 新增 `include/test_teptpl.h`、`include/impl/teptpl_test_helpers.hpp` 和 `include/impl/teptpl_assertions.hpp`。 |
| script and bench registry | 当前没有 topic-local script；bench case registry 在 `src/bench_teptpl.cpp`。 | 本阶段不新增分析脚本。 | adopted for layout | bench registry 随 bench harness 内移，label 不改。 | 只移动 registry 代码，不改 registry 语义。 |
| topic-local docs | Phase 040 已补 README / testing / correctness / benchmark / optimization evidence / code map。 | 本阶段需要刷新引用路径和恢复队列。 | adopted | 文档必须反映 `include/impl` 和 split source。 | 更新 README、code map、correctness / benchmark docs、roadmap、matrix、phase README。 |
| long-term docs | `doc-rvv` 仍写 current block 保留在 `test_support/`。 | 长期文档只保留当前 production 行为和证据链。 | adopted narrow | layout-only 更新，不改 production 行为。 | 把路径改为 `include/impl`，并标注 current block 仍是 test-rvv diagnostic baseline。 |
| legacy compatibility | 根目录旧 cpp / alias / evaluation 已在前序 phase 删除；旧 `test_support/` 本阶段将不再作为 include 入口。 | 配置默认不保留 compatibility alias。 | adopted | 未发现当前 Makefile、source、docs 需要旧 `test_support/` include。 | 删除旧路径引用；不新增 alias。 |

## Roadmap Default Recovery Queue

| queue item | 范围 | 状态 | 是否可合并 | 本阶段处理 |
| --- | --- | --- | --- | --- |
| `050-structure-layout/test-source-split` | 当前 topic `src/test_teptpl.cpp`、`src/bench_teptpl.cpp`、Makefile `SRCS_*`。 | `phase_deferred + unblocked` | 可与 internal-helper-layout 合并，因为二者共享 include graph。 | adopted。 |
| `050-structure-layout/internal-helper-layout` | 当前 topic `test_support/*.hpp` -> `include/impl/*.hpp`、聚合头和文档引用。 | `phase_deferred + unblocked` | 可与 test-source-split 合并，先迁内部头再拆源文件。 | adopted。 |
| production helper shape review | production RVV helper 重复 load/formula。 | `turn_stop_deferred` | 不与 layout-only 阶段合并。 | 不触碰 production API / production helper。 |
| row-source 扩展 | source-indexed、dual-indices、correspondences production 化。 | `turn_stop_deferred` | 会扩大 production 范围。 | 不做。 |

`ready_for_review_validity_checked` 是 Phase 040 的检查标签，不是本阶段 stop condition。由于队列仍有当前 topic 授权范围内的 `phase_deferred + unblocked` 结构动作，本阶段必须继续。

## 依赖顺序

1. 创建 050 plan。
2. 迁移旧内部 helper：`test_support/*.hpp` -> `include/impl/teptpl_*.hpp`，更新 `include/teptpl.h` 和内部 include。
3. 新增 test 专用聚合入口和 helper：`include/test_teptpl.h`、`include/impl/teptpl_test_helpers.hpp`、`include/impl/teptpl_assertions.hpp`。
4. 拆分 gtest 源文件，并更新 `SRCS_TEST` 为多文件列表。
5. 新增 bench 专用聚合入口和 bench impl 头，把 `src/bench_teptpl.cpp` 收敛为薄入口。
6. 更新 README、topic-local docs、phase README、optimization roadmap、optimization matrix、evaluation 和 `doc-rvv` 长期文档中的路径和恢复队列。
7. 运行 layout-only 验证。

## 验证计划

| validation | 命令 / 范围 | 完成判据 |
| --- | --- | --- |
| whitespace | `git diff --check -- test-rvv/registration/transformation_estimation_point_to_plane_lls doc-rvv/registration/transformation_estimation_point_to_plane_lls-RVV.zh.md` | pass。 |
| registry freshness | `python3 test-rvv/script/evidence_registry.py check ... --require-doc-ref --fail-on any` | fresh；本阶段不生成新 board summary。 |
| std correctness | `make -C test-rvv/registration/transformation_estimation_point_to_plane_lls run_test_std TEST_STD_OUTPUT_FILE=/tmp/teptpl_phase050_run_test_std.log` | 40/40 pass；临时日志不覆盖 registry-tracked logs。 |
| RVV correctness | `make -C test-rvv/registration/transformation_estimation_point_to_plane_lls run_test_rvv TEST_RVV_OUTPUT_FILE=/tmp/teptpl_phase050_run_test_rvv.log` | 40/40 pass；临时日志不覆盖 registry-tracked logs。 |
| std bench compile smoke | 构建 std bench binary，不运行完整 bench matrix。 | compile pass；不产生性能结论。 |
| RVV bench compile / asm smoke | `dump_bench_rvv` 或等价 RVV bench 编译。 | compile / asm dump pass；不作为新热点归因。 |

## 完成条件

- `test_support/` 不再作为当前 topic 常规内部 helper include 路径；内部头位于 `include/impl/`。
- `src/test_teptpl.cpp` 不再是单个 1500+ 行 gtest 源；Makefile 使用多源 `SRCS_TEST`，target 名不变。
- `src/bench_teptpl.cpp` 收敛为薄入口；bench fixture / component / case registry 位于 `include/impl/`。
- gtest case 名、bench label、case-filter 字符串和 production boundary 不变。
- 文档和 roadmap 不再把 `test-source-split` 或 `internal-helper-layout` 写成未阻塞 deferred。
- 通过本阶段验证；若验证失败，result 记录失败点和恢复命令。

## 停止 / 继续条件

本阶段闭合后，若只剩 production helper shape review、row-source production 扩展、更多点类型 / `Scalar` 或板卡性能扩展，这些都需要扩大到 production 实现或新增证据范围，默认不在本 layout-only phase 连续推进。若本阶段验证通过，`next_phase_default` 可以恢复为 review，但必须在 result 中重新执行 `ready_for_review_validity_check`。
