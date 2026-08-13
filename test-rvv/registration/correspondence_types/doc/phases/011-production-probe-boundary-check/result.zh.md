# Phase 011 结果：production-probe-boundary-check

## 实际执行范围

本阶段按用户反馈新增并执行 production probe（生产路径探针）：先临时把 `getQueryIndices` /
`getMatchIndices` 接到真实 production helper（生产源码 helper）中的 `__RVV10__` RVV 分支，再用真实
public helper（公开 helper）做 correctness（正确性）、asm attribution（反汇编归属）和 5-run board
repeated benchmark（重复板卡性能测试）。

阶段结论是：production direct probe 仍然是 `negative` bucket（负向决策桶），不支持保留生产接入。
因此临时 production patch 已回退；当前 `registration/include/pcl/registration/impl/correspondence_types.hpp`
保持标量实现。

## 计划动作回填

| action | status | 命令 / 证据路径 | 结论 |
| --- | --- | --- | --- |
| P1 写阶段计划 | done | `doc/phases/011-production-probe-boundary-check/plan.zh.md` | 计划先于 production probe 修改存在。 |
| P2 production probe patch | done then rolled back | 临时在 `correspondence_types.hpp` 中新增 `__RVV10__` 下的 `extractCorrespondenceIndexRVV`，用 `vlse32` 跨步加载 index 字段、`vse32` 连续写出 | 只用于本阶段探针；因板卡负向已回退，不保留 production 改动。 |
| P3 production direct correctness | done | `make run_test_compare`；`make run_board_test fetch_board_logs` | QEMU Std/RVV 均 8/8 pass；板卡 8/8 pass。 |
| P4 production direct bench case | done | `production-index-extract` case-filter；`src/bench_correspondence_types.cpp` | bench label 已和 Phase 010 diagnostic `index-extract` 分开。 |
| P5 asm attribution | partial pass | `make dump_bench_rvv`，窗口中可见 `vlse32.v` / `vse32.v` 出现在 production case lambda 内联边界 | 因 helper 被 inline，未保留独立 production 符号；归属记录为 production case 内联边界。 |
| P6 board repeated production bench | done | `log/board/011-production-probe-boundary-check/production-index-extract/summary.md`、`evidence_manifest.json`、`evidence_doctor.md` | 5-run 全部主 case 为 `negative` bucket。 |
| P7 决策和文档回填 | done | 本文件、optimization matrix、roadmap、evaluation | 生产探针已尝试并回退；不发布 `doc-rvv`。 |

## 临时 Production Probe 形态

本阶段使用过的临时 production patch 只覆盖 index extraction（索引抽取）：

- 在 `__RVV10__` 下 include `riscv_vector.h`。
- 增加 `correspondenceIndexLayoutSupported()`，要求 `pcl::index_t` 是 `std::int32_t`、`pcl::Correspondence`
  是 standard-layout（标准布局），且 `index_query` / `index_match` offset 与 12-byte AoS 布局一致。
- 增加 `extractCorrespondenceIndexRVV()`，每个 VL chunk（可变向量长度分块）使用 `vlse32` 从
  `Correspondence` 字段跨步加载，再用 `vse32` 连续写入 `pcl::Indices`。
- `getQueryIndices` / `getMatchIndices` 在 RVV helper 返回 false 时自然 fallback（回退）到原标量字段抽取。
- `getCorDistMeanStd` 没有修改。

该 patch 没有 public API（公开接口）变更；它只用于本阶段 board probe。最终工作区已回退，所以当前源码不含
上述 RVV 分支。

## Correctness

QEMU correctness：

- 命令：`make run_test_compare`
- 结果：Std 8/8 pass，RVV 8/8 pass。
- 新增覆盖：`QueryIndicesProductionDirectMatchesScalarReference` 和
  `MatchIndicesProductionDirectMatchesScalarReference` 直接调用 `pcl::registration::getQueryIndices` /
  `getMatchIndices`，并与纯标量参考链路对拍。

板卡 correctness：

- 命令：`make run_board_test fetch_board_logs`
- 结果：8/8 pass。
- 当前 `log/board/run_test.log` 是 production patch 回退后的最新板卡测试；Phase 011 的 production probe
  性能日志保存在独立 run-labelled 目录中。

## Board Repeated Summary

本阶段使用 5 个 run-labelled 批次作为 bounded rerun budget（有界复跑预算）。每个 run 使用：

```bash
make run_board_bench_compare fetch_board_logs \
  BENCH_ARGS="--case-filter production-index-extract --iterations 20 --warmup-iterations 5"
```

`B/A = Std production helper ms / RVV production helper ms`，大于 1 表示 RVV 更快。

| case | speedup values | median | min | max | checksum | bucket |
| --- | --- | ---: | ---: | ---: | --- | --- |
| `match-index production 64K` | `1.003, 0.918, 0.983, 0.907, 1.018` | 0.983 | 0.907 | 1.018 | match | `negative` |
| `query+match production 256K` | `0.966, 0.953, 0.972, 0.954, 0.984` | 0.966 | 0.953 | 0.984 | match | `negative` |
| `query-index production 4K` | `0.877, 0.729, 0.863, 1.117, 0.895` | 0.877 | 0.729 | 1.117 | match | `negative` |

证据主归属：

- summary artifact（摘要证据产物）：`log/board/011-production-probe-boundary-check/production-index-extract/summary.md`
- manifest（本地机器输入，默认不提交）：`log/board/011-production-probe-boundary-check/production-index-extract/evidence_manifest.json`
- Evidence Doctor：`log/board/011-production-probe-boundary-check/production-index-extract/evidence_doctor.md`
- raw run logs（原始运行日志）：`run-001` 到 `run-005`，默认只作为本机证据，不进入提交边界。

## Evidence Doctor 结果

Evidence Doctor 输入为 `log/board/011-production-probe-boundary-check/production-index-extract/evidence_manifest.json`，
输出为 `log/board/011-production-probe-boundary-check/production-index-extract/evidence_doctor.md`。

| severity | signal | case | 处理动作 |
| --- | --- | --- | --- |
| Error | `ba_degradation_frequency` | `match-index production 64K` | 3/5 低于 1，median 0.983；不能写 production speedup。 |
| Error | `ba_degradation_frequency` | `query+match production 256K` | 5/5 低于 1，median 0.966；作为生产接入拒绝信号。 |
| Error | `ba_degradation_frequency` | `query-index production 4K` | 4/5 低于 1，median 0.877；小规模长尾不支持保留 RVV 分支。 |
| Warning | `long_tail_or_variance` | `query-index production 4K` | 保留 min/median/max，不剔除 run-004 的正向离群；主结论仍按 5-run bucket 降级。 |

这些 Error 不表示 correctness bug（正确性缺陷）。它们表示当前 production direct probe 不能支撑 production
performance（生产性能）或生产接入保留。

## 分层证据结论

| 证据层 | 状态 | 说明 |
| --- | --- | --- |
| correctness | pass | QEMU 和板卡 correctness 均 8/8 pass。 |
| QEMU path / log shape | pass as smoke | QEMU 只证明构建和输出一致，不用于性能结论。 |
| asm attribution | partial pass | 临时 production patch 下的 bench 二进制出现 `vlse32.v` / `vse32.v`，归属为 production case 内联边界；无独立 production 符号。 |
| board performance | negative | 5-run production direct probe 三个 case 均为 `negative` bucket。 |
| production decision | rollback/no-production | 临时 production patch 已回退，当前 production 保持标量。 |

## 与 Phase 010 的关系

Phase 010 的 diagnostic negative 不能直接当作“禁止 production probe”的最终规则；因此本阶段新增了真实
production helper 边界的 probe。Phase 011 的结果表明：在当前 `correspondence_types` topic 中，边界补齐后
结论没有反转，真实 production helper 上仍不支持接入。

这和 historical weighted topic 的经验一致：diagnostic negative 需要同边界 production probe 复核；如果复核仍为
negative，就应回退 production 改动，而不是发布长期 production 文档。

## Evidence Freshness 和 Registry

本阶段复跑改变了 roadmap 中“默认不进入 production probe”的状态：现在 production probe 已尝试，并由
production direct board evidence 进一步确认 no-production。当前应刷新的 topic-local 文档包括：

- `doc/phases/README.zh.md`
- `doc/phases/optimization-matrix.zh.md`
- `doc/optimization-roadmap.zh.md`
- `doc/correspondence_types-evaluation.zh.md`

`log/evidence_registry.json` 尚未接入，`evidence_registry_status=not_available`。人工 freshness 检查路径：

- Phase 011 summary：`log/board/011-production-probe-boundary-check/production-index-extract/summary.md`
- Phase 011 manifest（本地机器输入，默认不提交）：`log/board/011-production-probe-boundary-check/production-index-extract/evidence_manifest.json`
- Phase 011 doctor：`log/board/011-production-probe-boundary-check/production-index-extract/evidence_doctor.md`
- 最新 board correctness：`log/board/run_test.log`

## 继续 / 停止决策

当前 phase 完成，EvidenceDecision 保持 `rollback/no-production`。停止命中条件：

- production direct 5-run board repeated summary 三个 case 全部为 `negative` bucket。
- Evidence Doctor 报告 3 个退化频率 Error 和 1 个长尾 Warning。
- 临时 production patch 已回退，当前 production 源码没有 RVV 分支。
- `getCorDistMeanStd` 仍带数值规约风险，且 index extraction 的真实生产边界已负向，默认不继续扩大。
- 当前 topic-local phase 文档、matrix、roadmap、evaluation 和 board summary 已同步 Phase 011 证据。

`next_phase_default`：`ready_for_review_no_production`。若用户仍想继续当前 topic，唯一推荐的下一阶段是
profiling / ablation（性能剖析 / 消融）解释负向来源，例如小规模 gate、`vsetvli` 开销、内存带宽或
跨步加载成本；它不再作为默认生产接入路径。
