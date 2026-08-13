# correspondence_types RVV 函数级评估

## 范围和目标源码

本评估覆盖 `registration/include/pcl/registration/impl/correspondence_types.hpp` 中三个 helper（辅助函数）：

- `getCorDistMeanStd`：统计 `pcl::Correspondences` 中 `distance` 字段的均值和样本标准差。
- `getQueryIndices`：按输入顺序抽取 `index_query`。
- `getMatchIndices`：按输入顺序抽取 `index_match`。

公开声明位于 `registration/include/pcl/registration/correspondence_types.h`，输入容器类型 `pcl::Correspondences` 和元素布局定义在 `common/include/pcl/correspondence.h`。

## 函数级结论

当前结论是 `rollback/no-production`（回退 / 不接入生产）：本 topic 已建立 test-rvv（RVV 测试资产）候选、QEMU correctness（QEMU 正确性验证）、QEMU bench smoke（QEMU 小型性能测试日志形状验证）、asm smoke（反汇编路径验证）、diagnostic board repeated benchmark（诊断板卡重复性能测试）、production direct probe（真实生产路径探针）和 Evidence Doctor（证据体检）manifest（证据清单）。Phase 011 曾按历史经验临时接入 `getQueryIndices` / `getMatchIndices` production helper 做真实源码边界测试，但 5-run board evidence 仍低于 `weak_positive` 门槛；临时 production patch 已回退，当前 production 源码保持标量。

`getQueryIndices` / `getMatchIndices` 是低风险 index extraction 候选：它们只读固定 offset 的 32-bit 字段并连续写出，语义边界清晰。Phase 010 diagnostic board 5-run 中 64K match、256K query+match 和 4K query case 的 median speedup 分别为 0.959、0.960、0.987，全部为 `negative` decision bucket（负向决策桶）。由于 diagnostic negative（诊断负向）不能直接禁止 production probe，Phase 011 又补了真实 production helper 边界；production direct 5-run 的 median 分别为 0.983、0.966、0.877，仍全部为 `negative`。因此不保留 production integration（生产接入）改动。

`getCorDistMeanStd` 是 reduction（规约）候选，但当前只保留 diagnostic。标量源码的 `distance * distance` 先以 float 计算，再转成 double 累加；candidate 保持 float-square same-chain（同构链路）和标量累加顺序，不把向量规约树接入 production。

## 函数族评估表

| 函数 | 标量路径 | 可 RVV 化片段 | 当前判断 | 需要补的证据 |
| --- | --- | --- | --- | --- |
| `getQueryIndices` | `resize(size)` 后按 `i` 顺序读取 `correspondences[i].index_query` 写到 `indices[i]` | `vlse32` 从 AoS（结构数组）字段跨步加载，`vse32` 连续写出 | `rejected_after_production_probe_negative`；diagnostic 和 production direct 两个边界均负向，临时生产 patch 已回退 | 默认无下一证据需求；若用户要解释负向，可另开 load/store / `vsetvli` / bandwidth / small-size gate 消融 |
| `getMatchIndices` | 同上，字段换成 `index_match` | 同上 | `rejected_after_production_probe_negative`；`UNAVAILABLE=-1` sentinel（哨兵值）保持，production direct 仍负向 | 同上 |
| `getCorDistMeanStd` | 空输入直接返回；否则用 `double sum` 和 `double sq_sum`；方差分母是 `size - 1` | 跨步加载 float distance，RVV 生成 chunk 内平方，再按标量顺序 double 累加 | `not_run_after_index_negative_no_production`；不接 production | 只有用户明确要求继续 reduction 候选时，才补板卡 A/B、数值预算和 production direct 证据 |

## 标量流程与 RVV 流程对照

| 阶段 | 标量流程 | 候选 RVV 流程 | 保留边界 |
| --- | --- | --- | --- |
| 输入大小 | 三个 helper 都以 `correspondences.size()` 为循环上限 | 相同 | 空输入语义必须保持 |
| index 抽取 | 每轮读取一个 `Correspondence` 字段 | 每个 VL chunk（可变向量长度分块）按 `sizeof(Correspondence)` 跨步读取字段，再连续写出 | 输出顺序、重复值和 `-1` sentinel 不变 |
| distance 统计 | 累加 `distance` 和 `distance * distance` | RVV 跨步加载 distance，做 float-square staging（单精度平方暂存），再存回临时数组给标量顺序累加 | `sqrt` 和 `size - 1` 分母保持标量；不批准 production vector reduction |

## Traceability Map

| 符号 / 文件 | 层级 | 作用 | 调用者 / 上游入口 | 被调用者 / 下游消费者 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- | --- | --- |
| `getCorDistMeanStd` | production public helper | 统计 correspondence distance 均值和样本标准差 | 用户代码或 registration helper | 输出 `mean` / `stddev` 引用参数 | production boundary（生产边界），本阶段不修改 | `registration/include/pcl/registration/impl/correspondence_types.hpp` |
| `getQueryIndices` | production public helper | 抽取 query/source index | 用户代码或 registration helper | `pcl::Indices` 输出 | production boundary；Phase 011 临时接入 probe 后因负向回退 | 同上 |
| `getMatchIndices` | production public helper | 抽取 match/target index | 用户代码或 registration helper | `pcl::Indices` 输出 | production boundary；Phase 011 临时接入 probe 后因负向回退 | 同上 |
| `rvv_correspondence_types_support` | test support | 保存 reference（参考链路）和 RVV candidate | `src/test_correspondence_types.cpp`、`src/bench_correspondence_types.cpp` | gtest / bench | correctness gate（正确性验收）和 QEMU smoke | `test-rvv/registration/correspondence_types/include/` |
| `generate_qemu_evidence_manifest.py` | topic-local script | 把 QEMU smoke 日志转成 Evidence Doctor manifest | `make run_evidence_doctor_qemu` | `log/qemu/evidence_manifest.json` | evidence metadata（证据元数据）补齐，不升级性能结论 | `test-rvv/registration/correspondence_types/script/generate_qemu_evidence_manifest.py` |
| `run_test_compare` | test target | std/RVV 两种构建运行专项 gtest | topic Makefile | `log/qemu/run_test_std.log`、`run_test_rvv.log` | QEMU correctness，不是性能证据 | `test-rvv/registration/correspondence_types/Makefile` |
| `run_bench_compare` | historical qemu smoke target | 生成可解析 QEMU bench smoke | topic Makefile | `log/qemu/analyze_bench_compare.log` | `qemu_smoke_only`，不能支撑性能结论 | 同上 |
| `run_evidence_doctor_qemu` | evidence target | 生成 manifest 并运行 Evidence Doctor | topic Makefile | `log/qemu/evidence_manifest.json`、`evidence_doctor.md` | 证据合同检查 | 同上 |
| `dump_bench_rvv` | asm target | 导出 RVV bench 二进制反汇编 | topic Makefile | `build/asm/riscv/bench_correspondence_types_rvv.asm` | asm smoke，production 归属未闭合 | 同上 |
| `generate_board_repeated_summary.py` | analysis script | 汇总 run-labelled board bench，并生成 board manifest / doctor | `make run_evidence_doctor_board_index_extract` | `log/board/010-board-diagnostic/index-extract/summary.md`、`evidence_manifest.json`、`evidence_doctor.md` | board diagnostic summary（板卡诊断摘要）和 no-production 决策输入 | `test-rvv/registration/correspondence_types/script/generate_board_repeated_summary.py` |
| `run_board_bench_compare` repeated runs | board bench | 在板卡上运行 Std/RVV 同边界 index extraction 对比 | topic Makefile + `board.mk` | `log/board/010-board-diagnostic/index-extract/run-001` 到 `run-005` | raw board evidence（原始板卡证据），默认不提交 | `test-rvv/registration/correspondence_types/log/board/010-board-diagnostic/index-extract/` |
| `production-index-extract` bench case | production direct probe | 临时 production patch 期间直接调用真实 `getQueryIndices` / `getMatchIndices` | `src/bench_correspondence_types.cpp` | `log/board/011-production-probe-boundary-check/production-index-extract/summary.md` | production direct probe，不是最终采用证据 | `test-rvv/registration/correspondence_types/log/board/011-production-probe-boundary-check/production-index-extract/` |

## 实现方式审计

| 实现维度 | 当前状态 | 证据 | 边界 / 恢复条件 |
| --- | --- | --- | --- |
| dispatch / fallback | `attempted_and_rolled_back_after_negative` | Phase 011 临时接入 production helper；QEMU/board correctness 通过，但 production direct board 5-run 仍负向 | 当前 production 保持标量；只有新的 code shape 在同 production boundary 达到 `weak_positive`，且用户授权，才重新尝试 |
| layout gate | `attempted_diagnostic` | `include/impl/correspondence_types_candidates.hpp` 中 `correspondence_layout_supported()` 检查 `pcl::index_t`、standard-layout 和 field offset | 若 layout 不满足，candidate fallback 到标量参考 |
| index extraction | `rejected_after_production_probe_negative` | QEMU correctness 通过；diagnostic board median 为 0.959、0.960、0.987；production direct board median 为 0.983、0.966、0.877 | 不接 production；后续只在用户要求性能归因时做 profiling / ablation |
| distance reduction | `attempted_diagnostic_no_production` | correctness 通过；same-chain 保持；asm 有 `vfmul.vv`，`vfwredosum.vs` 记录为自动向量化信号 | 不用 QEMU timing 或单个 asm 信号批准 production reduction |
| evidence manifest | `adopted_for_qemu_smoke` | `script/generate_qemu_evidence_manifest.py`、`log/qemu/evidence_manifest.json` | 仅覆盖 QEMU smoke，不承担性能结论 |
| board manifest | `adopted_for_board_repeated` | `script/generate_board_repeated_summary.py`、Phase 010 diagnostic manifest、Phase 011 production probe manifest | manifest 是 Evidence Doctor 的本地机器输入，默认不提交；summary / doctor 是摘要证据 |
| production scope | `rejected_no_production_after_probe` | production direct probe negative；临时 production patch 已回退 | 当前不改 production，不发布 `doc-rvv` |

## 测试和 bench 结果

| 测试 / target | 层级 | 结果 | 证据路径 |
| --- | --- | --- | --- |
| `CorrespondenceTypes.QueryIndicesCandidateKeepsInputOrder` | unit / boundary | pass | `log/qemu/run_test_std.log`、`run_test_rvv.log` |
| `CorrespondenceTypes.MatchIndicesCandidateKeepsSentinelAndOrder` | unit / boundary | pass | 同上 |
| `CorrespondenceTypes.QueryIndicesProductionDirectMatchesScalarReference` | production direct correctness | pass | 同上；板卡 `log/board/run_test.log` |
| `CorrespondenceTypes.MatchIndicesProductionDirectMatchesScalarReference` | production direct correctness | pass | 同上；板卡 `log/board/run_test.log` |
| `CorrespondenceTypes.EmptyInputDoesNotUseRVV` | fallback / boundary | pass | 同上 |
| `CorrespondenceTypes.DistanceStatsMatchesStdForRepresentativeInputs` | numerical consistency（数值一致性） | pass | 同上 |
| `CorrespondenceTypes.DistanceStatsStressInputMatchesStd` | numerical consistency / adversarial（对抗样本） | pass | 同上 |
| `CorrespondenceTypes.DistanceStatsDocumentsSingleElementBoundary` | public input semantics（公开输入语义） | pass | 同上 |
| `run_bench_compare` | QEMU bench smoke | checksum 一致；timing 不采信性能 | `log/qemu/analyze_bench_compare.log` |
| `dump_bench_rvv` | asm smoke | `vlse32.v`、`vse32.v`、`vfmul.vv` 存在；production 归属未闭合 | `build/asm/riscv/bench_correspondence_types_rvv.asm` |
| `run_evidence_doctor_qemu` | evidence doctor | Errors=0，Warnings=0，Suggestions=0 | `log/qemu/evidence_doctor.md` |
| `run_board_test fetch_board_logs` | board correctness | RVV test 8/8 passed | `log/board/run_test.log` |
| 5-run diagnostic `run_board_bench_compare` | board performance | `strided-index-extract` 三个 candidate case 均为 `negative` bucket；checksum match | `log/board/010-board-diagnostic/index-extract/summary.md` |
| `run_evidence_doctor_board_index_extract` | board Evidence Doctor | Errors=3，Warnings=1，Suggestions=0；退化频率作为 no-production 降级信号 | `log/board/010-board-diagnostic/index-extract/evidence_doctor.md` |
| 5-run production probe `run_board_bench_compare` | production direct probe | `production-index-extract` 三个 case 均为 `negative` bucket；checksum match | `log/board/011-production-probe-boundary-check/production-index-extract/summary.md` |
| `run_evidence_doctor_board_production_index_extract` | production probe Evidence Doctor | Errors=3，Warnings=1，Suggestions=0；退化频率触发回退 | `log/board/011-production-probe-boundary-check/production-index-extract/evidence_doctor.md` |

## Bench 说明

bench 输入是 synthetic `pcl::Correspondence` arrays（合成对应关系数组），覆盖 4K、64K 和 256K 规模的 query/match 抽取，以及 64K、256K 规模的 distance stats。`index-extract` case-filter 计时边界包含 test-only candidate 调用和 checksum；`production-index-extract` case-filter 在 Phase 011 临时 patch 期间直接调用真实 production helper，用来证明真实 helper 边界下的 public Std/RVV 差异。

QEMU smoke 的 Std/RVV checksum 一致，说明当前 checksum policy（校验和口径）下输出一致。QEMU timing 只保留在 `log/qemu/analyze_bench_compare.log` 中作为日志形状和路径证据，不能用于性能排序或 EvidenceDecision。

板卡 repeated summary 有两个主归属：Phase 010 diagnostic 使用 `log/board/010-board-diagnostic/index-extract/summary.md`；Phase 011 production probe 使用 `log/board/011-production-probe-boundary-check/production-index-extract/summary.md`。两者都覆盖 5 个 run-labelled 批次，`B/A = Std ms / RVV ms`，大于 1 表示 RVV 更快。Phase 011 的 production direct median 为 0.983、0.966、0.877，Evidence Doctor 对三条 case 报告 `ba_degradation_frequency`，因此真实源码边界也不支持 production 接入。

## 生产接入判断

当前不保留 production integration loop。原因：

1. Phase 010 diagnostic board 5-run repeated benchmark 已经显示 index extraction 负向。
2. Phase 011 又按真实 production helper 边界补 production direct probe；三条 case 仍为 `negative` bucket，且 Evidence Doctor 报告 3 个退化频率 Error。
3. 临时 production patch 的 asm attribution 只能归属到 inline bench lambda 边界，没有独立 production 符号；性能证据已经足够负向，不需要为了保留补更复杂归属。
4. 三个 helper 都是 inline 小函数，当前 RVV 跨步加载的收益不能覆盖指令、分流和维护成本。
5. distance 统计存在 reduction 数值预算，当前只验证 same-chain 诊断，不批准改变 production 累加树。

## 文档归属矩阵

| 信息类型 | 主归属 | 当前路径 |
| --- | --- | --- |
| 阶段计划和结果 | phase docs | `doc/phases/000-current-state-and-gaps/plan.zh.md`、`result.zh.md` |
| 优化矩阵 | phase docs | `doc/phases/optimization-matrix.zh.md` |
| 候选路线和恢复条件 | optimization roadmap | `doc/optimization-roadmap.zh.md` |
| S2 函数级评估、测试计划、验证结果和生产判断 | evaluation | 本文件 |
| no-production 诊断证据链和生产接入判断 | evaluation + phase result | 本文件、`doc/phases/010-board-diagnostic-and-production-decision/result.zh.md`、`doc/phases/011-production-probe-boundary-check/result.zh.md` |
| QEMU smoke manifest / doctor | local manifest + evidence summary | `log/qemu/evidence_manifest.json` 默认不提交；`log/qemu/evidence_doctor.md` 是可读摘要证据 |
| Board repeated summary / manifest / doctor | evidence summary + local manifest | `log/board/010-board-diagnostic/index-extract/summary.md`、`evidence_doctor.md` 是可读摘要证据；`evidence_manifest.json` 默认不提交 |
| Production probe summary / manifest / doctor | evidence summary + local manifest | `log/board/011-production-probe-boundary-check/production-index-extract/summary.md`、`evidence_doctor.md` 是可读摘要证据；`evidence_manifest.json` 默认不提交 |

## 当前状态

Phase 011 已完成 production probe boundary check（生产路径探针边界复核）。默认下一步是 reviewer 审查当前 `rollback/no-production` 结论；production 源码保持标量，`doc-rvv` 主题长期文档仍不适用。若用户仍希望继续当前 topic，建议另开 profiling / ablation（性能剖析 / 消融）小阶段，只解释 `strided-index-extract` 负向来源，而不是默认进入生产接入闭环。
