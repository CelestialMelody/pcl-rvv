# Phase 015 Result：production integration for all row sources

> Superseded by Phase 016：本文件保留四类 production probe 的历史结果。
> 当前提交候选以 `016-production-adoption-scope-finalization/result.zh.md` 为准：
> 保留 ordered/source-indexed/dual-indexed 三类 production RVV，移除 correspondence production RVV。

## 当前结论

本阶段已按用户授权把四类 row-source policy（行来源策略）的 RVV 优化接入
`registration/include/pcl/registration/impl/transformation_estimation_dual_quaternion.hpp`，
并完成 production public entry（生产公开入口）同边界测试。

PI5 结论必须停在用户判断点：当前 patch 还不能视为 adopted production behavior（已采纳生产行为），
也不能由 worker 自动回滚。

按 board repeated 和 Evidence Doctor：

- `ordered-cloud-pair`：支持保留候选，production direct positive，doctor clean。
- `source-indexed-cloud-pair`：支持保留候选，production direct positive，doctor clean。
- `dual-indexed-cloud-pair`：支持保留候选，production direct positive，doctor clean。
- `correspondence-pair`：当前不支持保留候选，64K / 256K production direct negative，doctor 有 Error。

因此本阶段建议不是“整包采纳四类”，而是交给用户在 PI5 判断：
保留前三类并取消 correspondence 接入，或取消整包接入，或要求继续做 correspondence 专项修正后再判断。

## Production patch 摘要

| area | 当前实现 |
| --- | --- |
| RVV gate | 仅 `__RVV10__` 构建启用；`Scalar=float`；source / target 分别满足 `pcl::rvv::RVVXYZAoSFloatLayout`；dense；最小点数 32。 |
| ordered-cloud-pair | strided xyz load + C1/C2 f64 reduction。 |
| source-indexed-cloud-pair | source indexed gather + target strided load；要求 32-bit `pcl::index_t`、indices 非负且在 source 范围内、cloud byte offset 可用 u32 表达。 |
| dual-indexed-cloud-pair | source / target 双侧 indexed gather；两侧 indices 均检查范围和 u32 byte offset。 |
| correspondence-pair | query / match 从 `pcl::Correspondence` AoS 中按字段 stride 读取，再双侧 indexed gather；要求 `pcl::Correspondence` standard-layout。 |
| fallback | 任一 gate 失败均回到原 `ConstCloudIterator` 标量路径。 |
| 数值后段 | RVV 只替换逐点 C1/C2 累加；Eigen 4x4 solve 和输出矩阵构造保持标量。 |

本阶段没有改 public API。

## Correctness 和 path-hit

QEMU correctness：

- 命令：`make run_test_compare`
- Std：`28/28 passed`
- RVV：`33/33 passed`

RVV 构建新增 production 直连测试均通过：

- `ProductionRVVOrderedCloudPairPathHitMatchesScalar`
- `ProductionRVVSourceIndexedCloudPairPathHitMatchesScalar`
- `ProductionRVVDualIndexedCloudPairPathHitMatchesScalar`
- `ProductionRVVCorrespondencePairPathHitMatchesScalar`
- `ProductionRVVFallbackGatesRejectOutOfScope`

这些测试证明 RVV 构建中四类 production detail path 可命中，且 fallback gate 能拒绝当前范围外输入。

## QEMU smoke

命令：

```bash
make run_bench_std run_bench_rvv BENCH_ARGS='--case-filter production-public-row-sources --iterations 1 --warmup-iterations 0'
```

结果：12 个 production public row-source cases 的 Std / RVV checksum 全一致。
QEMU timing 不进入性能判断。

## ASM attribution

命令：`make dump_bench_rvv`

证据路径：

- `build/asm/riscv/bench_transformation_estimation_dual_quaternion_rvv.asm`
- `build/asm/riscv/bench_transformation_estimation_dual_quaternion_rvv.full.asm`

归因摘要：

| row source policy | public / detail symbol 归因 | RVV 指令证据 |
| --- | --- | --- |
| `ordered-cloud-pair` | public symbol `0x3f464` 内联 RVV loop | `vlse32.v`、`vfwcvt.f.f.v`、`vfredosum.vs` |
| `source-indexed-cloud-pair` | public symbol `0x3f8be` 内联 RVV loop | `vle32.v` index load、`vluxei32.v` gather、`vfwcvt.f.f.v`、`vfredosum.vs` |
| `dual-indexed-cloud-pair` | public symbol `0x3fed8` 内联 RVV loop | 双侧 `vle32.v` / `vluxei32.v`、`vfwcvt.f.f.v`、`vfredosum.vs` |
| `correspondence-pair` | public symbol `0x406d2` 调用 detail symbol `0x3d696` | correspondence index stream 的 `vlse32.v`、双侧 `vluxei32.v`、`vfwcvt.f.f.v`、`vfredosum.vs` |

## Board repeated

命令：

```bash
make run_board_bench_production_public_repeated
```

运行合同：

- device：`Milkv-Jupiter`
- runs：5
- iterations：20
- warm-up iterations：5
- case-filter：`production-public-row-sources`
- evidence role：`production_direct`

| row source policy | 4K median | 64K median | 256K median | overall bucket | Evidence Doctor |
| --- | ---: | ---: | ---: | --- | --- |
| `ordered-cloud-pair` | `3.291x` | `3.634x` | `3.637x` | `positive` | `0/0/0` |
| `source-indexed-cloud-pair` | `2.526x` | `2.649x` | `2.635x` | `positive` | `0/0/0` |
| `dual-indexed-cloud-pair` | `1.881x` | `1.695x` | `1.980x` | `positive` | `0/0/0` |
| `correspondence-pair` | `2.443x` | `0.746x` | `0.867x` | `negative` | `2/3/0` |

对应证据路径：

- `log/board/production_public_row_sources_repeated/summary.md`
- `log/board/production_public_row_sources_repeated/evidence_doctor.md`
- `log/board/production_public_source_indexed_cloud_pair_repeated/summary.md`
- `log/board/production_public_source_indexed_cloud_pair_repeated/evidence_doctor.md`
- `log/board/production_public_dual_indexed_cloud_pair_repeated/summary.md`
- `log/board/production_public_dual_indexed_cloud_pair_repeated/evidence_doctor.md`
- `log/board/production_public_correspondence_pair_repeated/summary.md`
- `log/board/production_public_correspondence_pair_repeated/evidence_doctor.md`

## Evidence Doctor 解释

前三类 row-source policy 均为 `Errors=0 / Warnings=0 / Suggestions=0`。

`correspondence-pair` 有阻塞性 Error：

- 64K：5 次中 4 次 B/A 低于 1，median `0.746x`。
- 256K：5 次中 5 次 B/A 低于 1，median `0.867x`。

按本 topic 的 production gate，任一 size 为 negative 或 Evidence Doctor 出 Error，不能把该
row-source policy 纳入生产保留结论。因此 correspondence production RVV path 当前应视为
`pending_user_confirmation_rollback_or_rework`。

## 阶段决定

`continue_stop_decision`：PI5 停止，等待用户确认。

可选决策：

1. 保留前三类 production RVV path，取消 `correspondence-pair` production RVV path；随后 worker 可按该决策修改 patch、重跑 production correctness / board repeated，并同步 topic-local 文档和适用的长期 `doc-rvv`。
2. 取消整包接入；随后 worker 可在用户授权后回滚 production patch，只保留 test-rvv 诊断证据。
3. 暂不取消 correspondence，继续做 correspondence production 专项修正；需要新 phase 专门分析为什么 test-rvv diagnostic positive 但 production public 64K / 256K negative。

当前 patch 原样保留，等待用户判断。
