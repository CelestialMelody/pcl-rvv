# transformation_estimation_point_to_plane_lls Optimization Evidence

## 本文职责

本文按 RVV 优化方式索引代码、test target、bench target、board evidence 和当前决策。它帮助 reviewer
回答“为什么当前采用 fused-formula block dispatch，为什么其它路线暂缓或拒绝”。跨阶段搜索空间仍以
`doc/optimization-roadmap.zh.md` 为主归属；本文只整理当前 evidence state（证据状态）。

## 候选族状态

| candidate family | 代码路径 | correctness target | bench / evidence | 当前决策 | 边界 / 下一步 |
| --- | --- | --- | --- | --- | --- |
| fused-formula block-reduction production dispatch | production helper `buildPointToPlaneLLSFullCloudBlockRVVFusedFormula`；test support direct helper `accumulate_candidate_full_block_fused_formula_reduction`。 | `ProductionFullCloud*`、`ProductionFusedFullCloud*`、near-cancellation、generic point tests。 | `production_dispatch_generic_representative_5run_summary.md`；production-symbol asm attribution。 | adopted within current boundary。 | 只覆盖 full-cloud f32 AoS layout-gated `Scalar=float` 和三类代表点型 board evidence。 |
| current block-reduction baseline | test support `accumulate_candidate_full_block_reduction`；historical production-shaped baseline。 | `FullCloudBlockReduction*` tests。 | historical rows in production dispatch / fused summary。 | historical baseline, not production selector。 | 只有 reviewer 要求 same-boundary A/B 时重跑。 |
| fused-reduction | test support fused-reduction helper。 | `FullCloudFusedReductionMatchesStdWithinBudget`。 | direct helper historical bench rows。 | attempted / not adopted。 | 早期寄存器压力和机器码形态不作为当前 hot path。 |
| grouped-reduction | test support grouped helper。 | grouped invalid lane / scale stress tests。 | direct helper historical bench rows。 | attempted / not adopted。 | 当前不继续，除非 helper shape review 需要重新比较。 |
| trusted-dense | trusted-dense full-cloud helper。 | `FullCloudTrustedDenseCandidateMatchesStd`。 | trusted-dense diagnostic bench rows。 | rejected for current boundary。 | public finite semantics 仍逐字段检查；恢复需 `is_dense` 合同审计和 invalid-lane 负向测试。 |
| public-entry-shaped block shim | bench-only public-entry-shaped wrapper。 | `FullCloudBlockReductionPublicEntryShapeMatchesPublicWithinBudget`。 | `lls public-entry-shaped full-cloud block-reduction pointnormal`。 | diagnostic only。 | 证明入口形态兼容，不替代 production dispatch。 |
| source-indexed diagnostic | row source policy + source-indexed candidate helpers。 | `SourceIndices*` tests。 | historical source-indices bench rows。 | no-production / historical diagnostic。 | 需要独立 profile、policy-specific candidate 和 board repeated evidence。 |
| dual-indices diagnostic | dual row source policy + candidate helpers。 | `DualIndices*` tests。 | historical dual-indices bench rows。 | no-production / historical diagnostic。 | 退化不能单因归因为 gather；需 profile 或消融。 |
| correspondences diagnostic | correspondence adapter + candidate helper。 | `Correspondence*` tests。 | historical correspondences bench rows。 | no-production / historical diagnostic。 | query/match 展开属于边界；不能继承 dual-indices 结论。 |
| `Scalar=double` | production fallback path。 | `ProductionFullCloudScalarDoubleFallbackSmoke`。 | no board RVV evidence。 | scalar fallback。 | double-specific RVV 需要独立数值预算和板卡证据。 |
| doc suite parity | topic-local docs。 | `git diff --check`、registry check，必要时 QEMU correctness。 | no performance evidence。 | adopted in Phase 040。 | 结构成熟度动作，不改变 production candidate。 |
| structure layout split | `src/test_teptpl_*.cpp`、`include/test_teptpl.h`、`include/bench_teptpl.h`、`include/impl/teptpl_*.hpp`。 | `run_test_std` / `run_test_rvv` 40/40；bench compile / asm smoke。 | no performance evidence。 | adopted in Phase 050。 | test-source-split 和 internal-helper-layout 已闭合；新增测试/bench 按当前分组落位。 |

## 当前采用的优化方式

当前 adopted 方式是 fused-formula block-reduction production dispatch：

1. public full-cloud overload 先做窄 gate：`__RVV10__`、`Scalar=float`、source xyz f32 AoS、target xyz+normal f32 AoS、规模/VLEN/byte-offset gate。
2. RVV helper 用 A/B/C/N 四组 block reduction（分块规约）构造 normal equation。
3. fused formula 改写逐点 `a/b/c/d` 计算树：`a/b/c` 使用 `vfmsac` 形态，`d` 使用 `nx*(dx-sx) + ny*(dy-sy) + nz*(dz-sz)`。
4. Eigen solve 和 4x4 matrix construct 保持标量。
5. gate 失败回到 `ConstCloudIterator` 标量 helper。

采用理由：

- production direct correctness 覆盖 public overload、`accepted_points`、`ATA/ATb`、matrix、invalid lane、near-cancellation、scale-stress 和 fallback。
- representative point type board 5-run summary 显示三类 production-dispatch case 均稳定正向。
- current block baseline 在部分 generic 256K rows 有低谷；fused formula summary 未见类似低谷。

## 证据边界

| 证据层 | 路径 / target | 支撑 | 不支撑 |
| --- | --- | --- | --- |
| gtest correctness | `run_test_compare`、`src/test_teptpl_public_semantics.cpp`、`src/test_teptpl_candidates.cpp`、`src/test_teptpl_production_direct.cpp`、`src/test_teptpl_row_sources.cpp` | 当前 full-cloud production candidate 的输出和中间态预算。 | 性能。 |
| QEMU bench log-shape | `run_bench_compare --case-filter production-dispatch` | case filter、checksum、日志解析。 | 目标硬件性能。 |
| asm attribution | `dump_bench_rvv` 和 evaluation 归属表。 | fused helper 和 public call site 的 RVV 指令形态。 | speedup。 |
| board performance | `output/board/production_dispatch_generic_representative_5run_summary.md` | 三类代表点型 production-dispatch 性能。 | 全部 gate-allowed 点型、其它 row source。 |
| evidence registry | `log/evidence_registry.json` | 证据 digest 和 doc ref freshness。 | correctness / performance 本身。 |

## 暂缓 / 拒绝路线恢复条件

| 路线 | 当前状态 | 恢复条件 |
| --- | --- | --- |
| production helper shape review | turn-stop deferred | reviewer 要求压缩 helper 或合入前维护门槛触发；会触碰 production hot path，需重跑 correctness，若机器码变化再补 asm / board。 |
| test source split | adopted in Phase 050 | 已拆分为 public semantics、candidates、production direct 和 row sources 四个 gtest 源；不再作为默认恢复队列项。 |
| internal helper layout | adopted in Phase 050 | 常规 test support 内部头已迁到 `include/impl/teptpl_*.hpp`；旧 `test_support/` 不再作为当前 include 路径。 |
| more f32 AoS point types | deferred | 某个特殊点型进入 release 风险；需补 production direct、asm 和 board sampling。 |
| source-indexed / dual / correspondences production | deferred | 有 policy-specific candidate、profile / component ablation、board repeated summary 和 Evidence Doctor 输入。 |
| `Scalar=double` | deferred | 有 double numerical budget、RVV helper、asm 和目标硬件证据。 |
| trusted-dense | rejected | public `is_dense` / finite semantics 重新审计完成，并有 invalid-lane 负向测试和 board A/B。 |
