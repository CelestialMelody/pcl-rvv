# Phase 030 Plan：source-indexed bench evidence calibration

## 阶段意图和边界

本阶段只校准 `source-indexed-family` 的 bench（性能测试）证据口径，不直接修改 production C++（生产源码）。目标是把旧的 single-run board smoke（单次板卡冒烟）从当前性能判断中降级，并准备一条可重复、带 warm-up（预热）的 board evidence（板卡证据）路径，用来重新判断 `block-fused-abcd-ilp` 是否值得进入后续 production-candidate investigation（生产候选调查）。

本阶段证明：

- `run_bench_source_indexed_family_compare`、`run_board_bench_source_indexed_family` 这类带 std / RVV 数值输出的 bench target 默认带 warm-up。
- `source-indexed-family` 的 full-estimate（完整估计）和 component no-solve（组件级不求解）在文档中必须拆清 sink（结果消耗）和 timer boundary（计时边界）。
- 旧的 `run_board_bench_source_indexed_family` 日志只能作为 historical diagnostic（历史诊断），不能永久拒绝 `block-fused-abcd-ilp`。
- 新增 dedicated repeated board target（专用重复板卡目标）后，下一步可以让用户在板卡上跑稳定口径。

本阶段不证明：

- `block-fused-abcd-ilp` 已经可以替换 source-indexed production path。
- QEMU timing（QEMU 计时）具有任何真实性能含义。
- dual-indices（双索引路径）或 correspondences（对应关系路径）可以直接进入 production。

## 当前状态清单

| row source policy | 当前 production 状态 | 当前 test-rvv / board 状态 | 缺口 |
| --- | --- | --- | --- |
| full-cloud | `block-reduction + A/B/C/N + fused-abcd-ilp` 已 adopted。 | repeated board production evidence 已有。 | 无本阶段动作。 |
| source-indexed | production 当前 adopted family 是 staged-gather / compressed-tail。 | Phase 010 已补 `source-indexed-family` staged-gather、block-baseline、block-fused-abcd-ilp diagnostic；旧 board smoke 无 warm-up，doctor 有 Errors / Warnings。 | 需要带 warm-up repeated board 重新校准；source-indexed-specific asm 仍缺。 |
| dual-indices | production 保持标量。 | Phase 020 已完成 family diagnostic negative。 | 无本阶段动作；不得直接 production。 |
| correspondences | production 保持标量。 | Phase 020 已完成 family diagnostic negative。 | 无本阶段动作；不得直接 production。 |

关键路径：

- Phase 010 result：`doc/phases/010-source-indexed-family-carry-over/result.zh.md`
- Phase 020 result：`doc/phases/020-dual-indices-correspondences-family-carry-over/result.zh.md`
- 旧 source-indexed-family board log：`log/board/run_board_bench_source_indexed_family/analyze_bench_compare.log`
- 旧 source-indexed-family doctor：`log/board/run_board_bench_source_indexed_family/evidence_doctor.md`
- production source-indexed summary：`log/board/production_source_indices_staged_gather/summary.md`

## 假设与候选族

| 候选族 | 要审计的假设 | 本阶段处理 |
| --- | --- | --- |
| staged-gather / compressed-tail | 当前 source-indexed production adopted family 仍由 production repeated board 支撑。 | 保持 production 状态，不用旧 family smoke 覆盖。 |
| block-baseline | full-cloud adopted family 的 block-reduction 形态可能在 source-indexed gather ingress 下有收益。 | 只保留 diagnostic；等待 repeated board。 |
| block-fused-abcd-ilp | fused formula（融合公式）和 ILP（指令级并行）可能在 full-cloud 正向，但在 source-indexed gather / sink / timer 边界下需重新审计。 | 重新打开诊断，不能用旧 no-warmup smoke 永久拒绝。 |
| component no-solve | 拆 normal-equation 构造成本，帮助解释 full estimate 与组件差异。 | 必须写成 component diagnostic，不能和 full estimate 直接做严格 B/A。 |

## Optimization Matrix

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| block-reduction / A/B/C/N / fused-abcd-ilp | full-cloud | representative point types / `float` / contiguous weights | production full-cloud public overload | `run_test_production_direct_compare`、`run_test_candidates_compare` | `collect_board_production_dispatch_repeated` | adopted repeated board | production-default asm exists | current production doctor | adopted | none |
| staged-gather / compressed-tail | source-indexed | representative point types / `float` / source indices + contiguous weights | production source-indexed public overload | `run_test_source_indices_compare` | `collect_board_production_source_indices_repeated` | adopted repeated board | source-indexed-specific asm warning | production source-indexed doctor | adopted with warnings | source-indexed-specific asm deferred |
| staged-gather | source-indexed | `PointNormal` / `float` / source indices | pre-production diagnostic helper | `run_test_source_indexed_family_compare` | `run_bench_source_indexed_family_compare` | old smoke historical; new repeated pending | missing | old doctor has errors/warnings | diagnostic baseline | run repeated board |
| block-baseline | source-indexed | `PointNormal` / `float` / source indices | pre-production diagnostic helper | `run_test_source_indexed_family_compare` | `run_bench_source_indexed_family_compare` | old smoke historical; new repeated pending | missing | old doctor has errors/warnings | reopened diagnostic | run repeated board |
| block-fused-abcd-ilp | source-indexed | `PointNormal` / `float` / source indices | pre-production diagnostic helper | `run_test_source_indexed_family_compare` | `run_bench_source_indexed_family_compare` | old smoke historical; new repeated pending | missing | old doctor has errors/warnings | reopened diagnostic | run repeated board |
| staged/block/fused family | dual-indices | `PointNormal` / `float` / dual source-target indices | test-rvv diagnostic only | `run_test_dual_correspondence_family_compare` | `run_board_bench_dual_correspondence_family` | diagnostic negative | missing | diagnostic doctor has Errors | no-production | none in this phase |
| staged/block/fused family | correspondences | `PointNormal` / `float` / correspondences query/match | test-rvv diagnostic only | `run_test_dual_correspondence_family_compare` | `run_board_bench_dual_correspondence_family` | diagnostic negative | missing | diagnostic doctor has Errors | no-production | none in this phase |

## 实现和测试动作

| 动作 | 产物 | 预期证据 | 完成判据 |
| --- | --- | --- | --- |
| C1 agent asset 规则核对 | `.agents/skills/rvv-test/*`、`rvv-workflow/*` 的相关规则 | warm-up 默认、QEMU bench 边界、mixed-sink 规则可定位。 | Handoff 记录读取资产和适用规则。 |
| C2 bench harness / analyzer / Makefile 对齐 | `include/impl/teptplw_bench_harness.hpp`、`include/impl/teptplw_bench_cases.hpp`、`Makefile`、analysis script | dry-run 显示 source-indexed family bench / board target 带 `--warmup-iterations 5`；full estimate sink 口径写清。 | `make -n` 和 `py_compile` 通过。 |
| C3 旧 source-indexed-family diagnostic 降级 | phase result、benchmark docs、testing docs | 旧 no-warmup smoke 标为 historical diagnostic。 | 文档不再把旧 smoke 写成永久拒绝。 |
| C4 dedicated repeated board target | `collect_board_source_indexed_family_repeated` | 输出路径为 `log/board/source_indexed_family_repeated/summary.md`。 | dry-run 可复核；真实 board run 留给用户或下一轮。 |
| C5 topic docs 更新 | README、testing overview、benchmark evidence、optimization evidence、evaluation | bench label、case-filter、sink、Evidence Doctor 边界清楚。 | 文档能回答用户对 case 名和代码位置的疑问。 |
| C6 Handoff Packet 更新 | current-handoff `.zh.md` / `.yaml` | Phase 030 partial、next action 和 dirty isolation 可恢复。 | 新对话能按 handoff 继续。 |

## Evidence Doctor 和 Registry 规则

旧输入：

- `log/board/run_board_bench_source_indexed_family/evidence_manifest.json`
- `log/board/run_board_bench_source_indexed_family/evidence_doctor.md`

旧 doctor 只用于说明 historical diagnostic 的异常边界。若新 `collect_board_source_indexed_family_repeated` 运行后改变方向、decision bucket、doctor 数量或证据角色，必须刷新：

- `doc/phases/030-source-indexed-bench-evidence-calibration/result.zh.md`
- `doc/benchmark-and-evidence.zh.md`
- `doc/optimization-evidence.zh.md`
- `doc/transformation_estimation_point_to_plane_lls_weighted-evaluation.zh.md`
- current handoff

如果 repeated summary 暂时没有 manifest / doctor wrapper，Handoff 必须写成 `metadata_incomplete / doctor wrapper pending`，不能把 summary 当成完整 Evidence Doctor pass。

## 板卡复跑预算和决策桶

本阶段默认 repeated board 配置：

| 字段 | 默认值 |
| --- | --- |
| size | `262144` |
| runs | `5` |
| iterations | `20` |
| warm-up iterations | `5` |
| output | `log/board/source_indexed_family_repeated/summary.md` |

决策桶：

- `positive`：`block-fused-abcd-ilp` 在 same-boundary repeated board 中稳定优于 `staged-gather` 和 `block-baseline`，且 doctor 没有阻塞 Error。
- `weak_positive`：平均正向但有 warning，需要 source-indexed-specific asm 和 production-candidate plan。
- `neutral`：接近 `1.0x` 或不同 case 方向不一致，不进入 production replacement。
- `negative`：稳定低于 baseline，不进入 production。
- `unstable`：复跑预算内跨桶摇摆，降级为 diagnostic pending 或交给用户判断。

## 阶段完成条件

本阶段可以 partial closeout（部分收尾）的条件：

- bench / board target wiring 已 dry-run 通过。
- 文档已经把 old no-warmup smoke 降级。
- Handoff 明确写出真实 board run 未完成，以及下一轮默认命令。

本阶段不能 closeout 为 production-candidate 的条件：

- 没有新的 repeated board summary。
- 没有 source-indexed-specific asm attribution。
- 没有 production direct / fallback plan。

## 继续 / 停止条件

默认下一步：

```bash
make -C test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted \
  collect_board_source_indexed_family_repeated
```

若用户确认要“先接入 production 再看 production bench”，必须另开新的 production-candidate phase plan，写清候选范围、fallback、production direct tests、asm attribution 和 board bench 预算；不能把本阶段的 diagnostic repeated target 直接写成 production replacement。

## 文档更新清单

- `doc/phases/030-source-indexed-bench-evidence-calibration/result.zh.md`
- `doc/phases/README.zh.md`
- `README.zh.md`
- `doc/testing-overview.zh.md`
- `doc/benchmark-and-evidence.zh.md`
- `doc/optimization-evidence.zh.md`
- `doc/transformation_estimation_point_to_plane_lls_weighted-evaluation.zh.md`
- `tmp/rvv-work-logs/registration/transformation_estimation_point_to_plane_lls_weighted/current-handoff/current-handoff.zh.md`
- `tmp/rvv-work-logs/registration/transformation_estimation_point_to_plane_lls_weighted/current-handoff/current-handoff.yaml`
