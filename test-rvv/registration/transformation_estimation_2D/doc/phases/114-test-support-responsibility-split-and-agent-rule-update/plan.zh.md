# TE2D 测试支撑职责拆分与 Agent 规则更新实施计划

> **For agentic workers:** 本计划只覆盖 `registration/transformation_estimation_2D` 的 test-rvv 测试支撑结构迁移。执行时逐项回填 `result.zh.md`，保持生产源码和已有性能结论不变。

**目标：** 将历史单一候选实现头按测试支撑职责拆成多个内部头文件，保留 `include/te2d.h` 作为唯一稳定聚合入口，并用正确性 / QEMU smoke（小型冒烟）证明拆分没有语义回归。

**架构：** `include/te2d.h` 继续是 test-rvv 测试和 bench 的唯一外部 include 入口；新的 `include/impl/te2d_*.hpp` 按依赖方向组织为 core types、fixtures、layout helpers、row sources、ordered/source-indexed/dual-indexed/correspondence candidates、family A/B wrappers 和 checksums。历史单一实现入口在同轮删除，不保留 compatibility aggregator（兼容聚合入口）或 legacy alias（旧别名）。

**技术栈：** C++17、PCL test-rvv、Eigen、GoogleTest、RVV intrinsics（RVV 内建函数）、现有 topic-local Makefile / QEMU harness。

**依据：** `AGENTS.md`、`.agents/config/defaults.yaml`、Phase 113 Handoff、`doc/phases/README.zh.md`、`doc/optimization-roadmap.zh.md`、`doc/phases/optimization-matrix.zh.md`、`doc/test-support-code-map.zh.md` 和已提交的 Commit A `e22032462`。

## 全局约束

- 不修改 production source（生产源码），不 amend、回滚或扩大现有 production gate。
- Phase 103/104 source-indexed generic widening 继续写成 guarded probe（受保护探针），不得写成 adopted。
- Phase 106 generic public variance 继续保持 negative；Phase 111 Normal 类不接入，本阶段不创建 Normal 优化候选。
- 不改 gtest / bench case 名称、Make target 名称、CLI case-filter、输出合同和计时边界。
- `include/te2d.h` 保持稳定；历史单一实现入口必须删除。
- 所有新内部头文件开头写简短中文职责说明，并说明它们只属于 RVV 测试支撑，不能证明 production dispatch。
- 新 include graph（包含图）只能向下依赖，避免内部头循环 include。
- Commit B 只允许包含 TE2D test support、topic-local docs、Phase 114 phase / roadmap / matrix 同步；不包含 `.agents`、production source、tmp handoff、raw/generated logs 或其它 dirty 文件。

## 当前状态与形态审计

| 形态 | 当前路径 | 已发现职责 | 未拆风险 | 本阶段决定 |
| --- | --- | --- | --- | --- |
| 稳定聚合入口 | `include/te2d.h` | 唯一测试 / bench include 入口 | 若入口改名会破坏 Makefile 和文档合同 | 保留并只改为聚合新内部头 |
| 单个大 helper header | 历史单一候选实现头，约 1707 行 | core types、fixtures、layout gate、row source、reference、RVV math、candidate、public wrapper、family A/B、checksums | reviewer 不能按职责定位；依赖关系隐藏；新增 row source 容易继续堆入同一文件 | 按职责拆分并删除旧文件 |
| `src` 测试 / bench | `src/test_te2d.cpp`、`src/bench_te2d.cpp` | case 清单、bench harness、输出合同 | 本阶段不扩大到源文件职责拆分 | 保持 case 名和 target 合同，只更新 include 经过聚合入口 |
| 旧 `test_support/` 目录 | 不存在 | 不适用 | 不能因为目录不存在而跳过 helper 审计 | 直接使用 `include/impl` 作为内部职责目录 |
| topic-local script / registry | `script/**`、`log/evidence_registry.json` | 证据摘要、manifest、registry | 旧路径引用可能进入代码地图或文档 | 扫描并更新所有旧 header 引用；不改证据内容 |
| topic-local docs | `README.zh.md`、`doc/*.zh.md`、phase docs | 测试入口、代码地图、阶段恢复 | 旧路径会让 reviewer 进入已删除文件 | 同轮更新代码地图、README、evaluation、roadmap、matrix、Phase README/Handoff |

## 职责与依赖设计

本阶段先按职责拆分，再用 include 依赖闭合。具体文件名可以在实现时按符号扫描微调，但不得退回单一大 header：

| 内部头文件 | 主要职责 | 允许依赖 |
| --- | --- | --- |
| `include/impl/te2d_core_types.hpp` | 公共 include、`CandidateStats`、`Fused2DAccumulation` 和共享类型 | PCL / Eigen / 标准库 |
| `include/impl/te2d_fixtures.hpp` | 点云、变换、额外字段和数值压力样本构造 | core types |
| `include/impl/te2d_layout_helpers.hpp` | layout gate、有限值 / dense 检查、索引合法性和统计填充 | core types |
| `include/impl/te2d_row_sources.hpp` | source-indexed、dual-indexed、correspondence 的索引计数、合法性、物化和索引展开 | core types、layout helpers |
| `include/impl/te2d_public_wrappers.hpp` | 真实 production public overload 的 test-only wrapper | core types |
| `include/impl/te2d_family_ab.hpp` | materialize-to-ordered、staged-dual 等同边界 family A/B 对照 wrapper | public wrappers、row sources |
| `include/impl/te2d_ordered_candidates.hpp` | ordered-cloud-pair 标量 / RVV accumulation、求解、ordered candidate | core types、layout helpers |
| `include/impl/te2d_source_indexed_candidates.hpp` | source-indexed materialize / direct gather 标量与 RVV candidate | core types、layout helpers、row sources、ordered candidates |
| `include/impl/te2d_dual_indexed_candidates.hpp` | dual-indexed materialize / direct gather 标量与 RVV candidate | core types、layout helpers、row sources、ordered candidates |
| `include/impl/te2d_correspondence_candidates.hpp` | correspondence direct gather、chunked staging 和 candidate wrapper | core types、layout helpers、row sources、ordered candidates |
| `include/impl/te2d_checksums.hpp` | 矩阵差异和 checksum helper | core types |
| `include/te2d.h` | 稳定聚合入口 | 按依赖顺序 include 上述内部头 |

实现时如果某个函数为了避免重复声明需要移动到相邻职责文件，必须在 `result.zh.md` 记录最终落点和依赖理由；不允许通过互相 include 解决循环依赖。

## 诊断到生产错配审计

| 问题 | 本阶段结论 |
| --- | --- |
| evidence role（证据角色） | `implementation-shape` 与 `correctness`；本阶段不生成性能决策证据 |
| A/B boundary（A/B 边界） | 同一 test-rvv wrapper 的旧单头 include graph 与新职责 include graph |
| 当前决策问题 | `fallback correctness`、`implementation-shape`；不是 RVV-vs-scalar 性能或 RVV-family-selection |
| 是否可外推到 production | 否。拆分只证明测试支撑行为和 include 关系保持，不能证明 production dispatch |
| comparison / baseline mismatch 风险 | 低；基线和候选都使用相同 test / bench case、输入、case-filter 和生产 wrapper，只有头文件组织变化 |
| 若 smoke 失败 | 先修复声明 / include / ODR（单一定义）问题；不得改 production 或算法实现 |
| clean adoption 是否需要 production detail A/B | 不适用；本阶段不改变 production family 或 gate |

## 执行动作与完成判据

| ID | 动作 | 产物 / 命令 | 完成判据 |
| --- | --- | --- | --- |
| P1 | 完成职责和符号归属扫描 | `rg`、行数、include 引用扫描；本计划的职责表 | 每个旧符号有唯一新文件归属，include 方向无循环候选 |
| P2 | 创建新内部头并迁移实现 | `include/impl/te2d_*.hpp`、更新 `include/te2d.h`、删除历史单一实现入口 | 新文件各自有职责说明；聚合入口可独立编译；旧文件不存在 |
| P3 | 清理旧路径引用 | 对已删除历史入口名做限定扫描，覆盖 topic、doc、Handoff、Makefile、script | 旧入口零引用；文档和 code map 全部指向新文件 |
| P4 | 保持测试合同 | `make -C test-rvv/registration/transformation_estimation_2D -n run_test_compare` 与现有 Make target / case-filter 扫描 | gtest / bench case 名、Make target、CLI 参数和输出合同不变 |
| P5 | 正确性验证 | `make -C test-rvv/registration/transformation_estimation_2D run_test_compare` | Std / RVV correctness 通过；生产源码 diff 为空 |
| P6 | QEMU smoke 验证 | `make -C test-rvv/registration/transformation_estimation_2D record_qemu_correctness_state`；必要时窄范围 `run_bench_source_indexed_pointxyzi_public_smoke` | QEMU 只作为 correctness / path / log-shape 证据；Evidence Doctor 无由拆分引入的 Error |
| P7 | 同步 topic 文档和状态 | `README.zh.md`、`doc/test-support-code-map.zh.md`、相关 evaluation / correctness / optimization 文档、phase README、roadmap、matrix、Handoff、Phase 114 result | 新结构、旧入口删除、证据边界和下一恢复动作可追溯 |
| P8 | 提交前检查并创建 Commit B | `evidence_status`、YAML parse、`git diff --check`、staged set 审计 | 只提交 TE2D test-support / topic-local docs；排除 `.agents`、production、tmp、logs 和无关 dirty 文件 |

## 测试与证据策略

- 本阶段不跑板卡性能，不刷新已有 board summary，不改变任何 EvidenceDecision。
- `run_test_compare` 是主 correctness gate（会失败的正确性验收）。
- QEMU 只验证重编译后的 Std / RVV 二进制、路径和日志形状；不把 QEMU timing 写成性能结论。
- Evidence Doctor 本阶段以已有 `record_qemu_*` / `evidence_status` 路径为主；若只做 correctness smoke，则在 result / Handoff 中记录人工检查边界，不伪造新的 board evidence。
- `tmp/rvv-work-logs/**`、`log/**`、`build/**` 和 raw/generated logs 保持本地，不纳入 Commit B。

## 阶段完成条件

1. 新的聚合入口能通过所有原有 test / bench include。
2. 历史单一实现入口已删除，且 topic、文档、Makefile、script、code map 和 Handoff 零旧引用。
3. 算法表达式、RVV intrinsic、fallback、stats、case 名、target 名、日志字段和生产源码均未改变。
4. `run_test_compare` 和至少一个 TE2D QEMU smoke 通过。
5. `evidence_status`、YAML parse、`git diff --check` 通过。
6. Phase result、README、roadmap、matrix、test-support code map 和 Handoff 记录 adopted 结构、未覆盖范围、旧入口删除结果和下一动作。

## 继续 / 停止条件

- 默认继续到 P8；测试支撑迁移、旧引用清理和 topic-local 文档同步都在本轮授权范围内。
- 如果 dirty isolation 变得不安全、发现外部脚本 / 非 topic 依赖无法同轮更新，或 correctness 失败且无法在不改算法语义的情况下修复，停止并写 `turn_stop_deferred`。
- 本阶段不得以“结构拆分完成”写成 topic production closeout；Phase 103/104、106、111 的 production 边界保持原状。
- Phase 114 结束后，若结构和文档没有新的 unblocked 缺口，默认恢复动作是 reviewer 检查 Commit B；不创建 Normal、correspondence 或 generic widening 优化 phase。
