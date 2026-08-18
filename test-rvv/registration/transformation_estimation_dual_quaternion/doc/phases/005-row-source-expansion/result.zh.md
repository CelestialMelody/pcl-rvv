# Phase 005 Result：row-source-expansion

## 当前结论

本阶段在 `test-rvv/registration/transformation_estimation_dual_quaternion` 内完成
source-indexed-cloud-pair（源索引点云对）、dual-indexed-cloud-pair（双索引点云对）和
correspondence-pair（对应关系点对）的 staged ordered reuse（先展开并暂存为顺序点云）
诊断。三类输入都使用非 identity 的确定性索引 / 对应关系，并通过同构标量参考和现有
ordered C1/C2 RVV candidate 对拍。

板卡 repeated 结果如下：

- source-indexed-cloud-pair：4K / 64K / 256K median B/A 为 `1.529x / 1.419x / 1.411x`，overall `positive`。
- dual-indexed-cloud-pair：4K / 64K / 256K median B/A 为 `1.180x / 1.286x / 1.160x`，overall `weak_positive`。
- correspondence-pair：4K / 64K / 256K median B/A 为 `1.223x / 1.095x / 1.100x`，overall `weak_positive`。

这些结果只证明“索引 / 对应关系展开、staging 和 ordered C1/C2 candidate 放在同一计时
边界内”的 test-rvv 诊断价值。它们不证明真实 production dispatch（生产分流）已经命中
RVV，也不证明可以直接把 staged helper 接入 production。当前 production TEDQ 头文件保持
不变，`doc-rvv` 仍不适用。

## 计划动作回填

| action | 状态 | 命令 / 产物 | 事实与结论 |
| --- | --- | --- | --- |
| A1 row source fixtures | done | `include/impl/tedq_candidates.hpp`、`src/test_tedq.cpp` | 已加入 deterministic non-identity source / target indices 和 correspondence；Std/RVV 各 14 个 gtest 通过。 |
| A2 staged candidates | done | `include/impl/tedq_candidates.hpp` | source-indexed、dual-indexed、correspondence 均先 materialize 为 ordered cloud，再复用现有 candidate；`CandidateStats.used_staging` 暴露 staging 边界。 |
| A3 correctness tests | done | `make run_test_compare` | Std/RVV 两个 QEMU 构建均为 14/14 通过；public 非 identity source-indexed、dual-indexed 和 correspondence 均与 staged scalar reference 一致。 |
| A4 bench filter | done | `src/bench_tedq.cpp`、`--case-filter row-source-expansion` | bench 同时输出三类 public 与 staged candidate case；index / correspondence 展开和 staging 落在 staged candidate 的计时范围内。 |
| A5 evidence wrapper | done | `Makefile`、`script/generate_tedq_board_repeated_summary.py` | 新增 board repeated target；同一 5-run 原始采集分别生成三份 policy-specific summary / manifest / doctor。 |
| A6 docs / matrix | done | topic-local doc suite、roadmap、matrix、本 result | 已更新 row-source policy 的独立证据、提交白名单、registry 引用和下一阶段恢复条件。 |

## 正确性和数据语义

`PublicSourceIndexedNonIdentityMatchesStagedScalar` 验证的 row pairing 是
`source[indices[i]] -> target[i]`。双索引路径验证 `source[source_indices[i]] ->
target[target_indices[i]]`；correspondence 路径验证 `index_query / index_match` 展开后的
同一 row 顺序。三类测试都使用固定刚体变换生成 target，避免 identity indices 把 row-source
语义错误隐藏起来。

Std / RVV correctness 日志：

- `log/qemu/run_test_std.log`：14 tests passed。
- `log/qemu/run_test_rvv.log`：14 tests passed。

这些日志是 ignored-local raw log（本机原始日志）；对应的 QEMU correctness 登记已刷新为
`qemu-tedq-correctness-phase005`。

## 板卡证据

采集合同为 5 runs、每次 20 iterations、5 warm-up iterations，设备为 `Milkv-Jupiter`。
同一次原始采集包含三类 row source case；摘要按 policy 独立登记，避免一个 policy 的
结论外推到其它 policy。

| row source policy | summary | manifest / doctor | overall bucket | 主要结论 |
| --- | --- | --- | --- | --- |
| source-indexed-cloud-pair | `log/board/row_source_expansion_repeated/summary.md` | 同目录 `evidence_manifest.json`、`evidence_doctor.md` | `positive` | staged ordered reuse 有稳定诊断收益。 |
| dual-indexed-cloud-pair | `log/board/row_source_expansion_dual_indexed_repeated/summary.md` | 同目录 `evidence_manifest.json`、`evidence_doctor.md` | `weak_positive` | 双侧 staging 仍有正向信号，但收益明显低于 source-indexed。 |
| correspondence-pair | `log/board/row_source_expansion_correspondence_repeated/summary.md` | 同目录 `evidence_manifest.json`、`evidence_doctor.md` | `weak_positive` | correspondence 展开后仍有小幅正向信号，不能视为 production 结论。 |

三份 Evidence Doctor 均为 `Errors=0，Warnings=0，Suggestions=0`。三类摘要的 Std/RVV
checksum 均一致。当前没有使用 QEMU timing 形成性能结论。

## EvidenceDecision 与 production 边界

本阶段的 EvidenceDecision 是：

`row_source_staged_ordered_reuse_diagnostic_positive_no_production`

它只批准三类 row-source policy 在 test-rvv diagnostic 中各自保留 staged candidate。
它不批准：

- 直接把 staged candidate 接入 production public overload；
- 用 source-indexed 的 positive 外推 dual-indexed 或 correspondence 的 production 价值；
- 用 dual-indexed / correspondence 的 weak-positive 取代同边界 production-family comparison；
- 把 board diagnostic summary 写成 production direct 或 production-ready。

若后续进入 production integration loop，必须沿用 Phase 004 的 PI1 合同，并新增
row-source-specific 的 path-hit、fallback、production asm attribution、production direct
board repeated 和 registry freshness 证据。当前不修改
`registration/include/pcl/registration/impl/transformation_estimation_dual_quaternion.hpp`。

## Evidence Doctor、registry 和 freshness

本阶段使用：

```bash
make run_test_compare
make record_qemu_correctness_state
make run_board_bench_row_source_expansion_repeated
make record_board_row_source_expansion_state
make evidence_status
```

`make evidence_status` 在文档刷新前会报告新增 summary 尚未被文档引用；补齐本 result、
README、benchmark / evidence、optimization evidence、evaluation 和 roadmap 后重新检查。
最终应为 `evidence registry check: fresh`。generated board raw logs 保持 local-only，
summary / manifest / doctor 按 `summary-only` 进入 review 候选。

## Optimization Matrix 更新

| candidate family | row source policy | correctness | board / doctor | decision | 下一动作 |
| --- | --- | --- | --- | --- | --- |
| staged ordered reuse | source-indexed-cloud-pair | 14-test Std/RVV pass；非 identity public 与 staged scalar 对拍通过 | 5-run `positive`；doctor 0/0/0 | `attempted_diagnostic_positive` | 进入 implementation-family comparison；未授权前不接 production。 |
| staged ordered reuse | dual-indexed-cloud-pair | 14-test Std/RVV pass；双侧非 identity staged 对拍通过 | 5-run `weak_positive`；doctor 0/0/0 | `attempted_diagnostic_weak_positive` | 比较双侧 staging 与直接 gather / 其它 family 的成本；仍保持 diagnostic。 |
| staged ordered reuse | correspondence-pair | 14-test Std/RVV pass；非 identity query/match staged 对拍通过 | 5-run `weak_positive`；doctor 0/0/0 | `attempted_diagnostic_weak_positive` | 审计 correspondence 展开、重复索引和输出语义；仍保持 diagnostic。 |
| production dispatch | 三类 non-ordered row source | 本阶段没有 production direct | not_applicable | `deferred_until_new_PI1_contract` | 只有用户 / reviewer 授权 PI1 后重开 production。 |

## 结构成熟度与继续判断

本阶段新增 row-source adapter、candidate 和 board evidence 职责后，
`include/impl/tedq_candidates.hpp` 达到 858 行，超过配置的 800 行 soft limit，但未达到
1000 行 hard limit。当前文件仍可编译和审查，但下一阶段应把 row-source fixtures / adapters
从候选与 reference 中拆出，降低后续 gather 或 direct production probe 的审查负担。

因此本阶段本身已闭合，但不能写成 `ready_for_review`。默认下一 phase 是
`006-test-support-split-and-row-source-family-comparison`，范围仍限 test-rvv：先拆分
`include/impl` 内部职责，再比较 staged ordered reuse 与 direct gather / 已有 ordered
candidate family。生产接入仍是独立授权边界。

## 提交边界

| 路径 | 状态 | 说明 |
| --- | --- | --- |
| `registration/include/pcl/registration/impl/transformation_estimation_dual_quaternion.hpp` | unchanged | production 源码未修改。 |
| `log/board/row_source_expansion_repeated/summary.md` 及 manifest / doctor | to-be-staged | source-indexed diagnostic summary。 |
| `log/board/row_source_expansion_dual_indexed_repeated/summary.md` 及 manifest / doctor | to-be-staged | dual-indexed diagnostic summary。 |
| `log/board/row_source_expansion_correspondence_repeated/summary.md` 及 manifest / doctor | to-be-staged | correspondence diagnostic summary。 |
| `log/board/**/run-*/*` | ignored-local | board raw logs，不默认提交。 |
| `build/`、板卡临时二进制、`config.mk` | ignored / excluded | 不进入 topic 提交边界。 |
