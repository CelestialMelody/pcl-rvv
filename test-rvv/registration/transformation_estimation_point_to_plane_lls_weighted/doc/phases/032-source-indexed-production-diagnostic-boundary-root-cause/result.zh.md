# Phase 032 Result：source-indexed production / diagnostic boundary root-cause

## 阶段结论

本阶段已经定位 Phase 030 负向与 Phase 031 正向分歧的主要原因：

```text
comparison-boundary / baseline mismatch
```

Phase 031 的 production probe 是可信的 production public Std/RVV 测试，但它回答的是：

```text
真实 public source-indexed overload 在启用 RVV 后是否快于同一 public overload 的非 RVV scalar path
```

它没有回答：

```text
source-indexed block-fused-abcd-ilp RVV 是否快于 source-indexed staged-gather RVV
```

本阶段新增的 production detail RVV-vs-RVV A/B 正好回答第二个问题。结果显示 block-fused 相对 staged
是 mixed / negative，尤其 262144 规模有多项明显负向。因此 Phase 030 的负向不是一个简单的
timer / warm-up / checksum bug；它是一个真实的 harness-risk signal，只是不能直接外推成
production public Std/RVV 结果。

当前更准确的结论是：

- 你的“跳过 030 的不建议接入、先做 production probe”想法是对的：它发现 Phase 030 不能直接阻止真实 production probe。
- 但 Phase 031 的正向不证明 block-fused 比 staged 更好；新 detail A/B 反而说明 block-fused 相对 staged 仍有风险。
- 因此 source-indexed block-fused 不能升级为 clean adopted。更合理的 production 决策是回到 staged-gather，或至少把 block-fused 降级为受控实验路径，直到同边界 A/B 转正。

## 具体根因链

### 1. Phase 030 测的是 test-rvv diagnostic helper

Phase 030 summary：

```text
log/board/source_indexed_family_repeated/summary.md
```

关键行：

| case | median | min | max | values |
| --- | ---: | ---: | ---: | --- |
| `source-indexed-family block-fused-abcd-ilp pointnormal 262144` | `0.90x` | `0.76x` | `1.20x` | `0.83x, 0.93x, 0.90x, 1.20x, 0.76x` |
| `component source-indexed-family block-fused-abcd-ilp no-solve 262144` | `0.98x` | `0.85x` | `1.18x` | `1.18x, 0.98x, 0.93x, 1.03x, 0.85x` |

该阶段已使用 5 runs、20 iterations、5 warm-up，并且 Phase 030 已修正 full estimate 的
normal-equation + matrix sink。因此这不是“没有 warm-up”或“计时函数明显错误”造成的负向。

但它的入口是 test-rvv implementation-family wrapper，不是 production public overload。

### 2. Phase 031 测的是 production public Std/RVV

Phase 031 summary：

```text
log/board/production_source_indices_block_fused_abcd_ilp_probe/summary.md
```

关键结果：

| case | median |
| --- | ---: |
| `production-dispatch source-indices pointnormal 262144` | `1.64x` |
| `production-dispatch source-indices pointnormal 65536` | `1.71x` |
| `production-dispatch source-indices pointxyz-to-pointnormal 262144` | `1.54x` |
| `production-dispatch source-indices pointxyz-to-pointnormal 65536` | `1.64x` |
| `production-dispatch source-indices pointxyz-to-pointxyzinormal 262144` | `1.69x` |
| `production-dispatch source-indices pointxyz-to-pointxyzinormal 65536` | `1.69x` |

这些 case 调用真实 public source-indexed overload。Std 侧是 public scalar iterator path，RVV 侧是
production RVV fast path。它可以证明“接入 RVV 后比 public scalar path 快”，但不能证明
`block-fused-abcd-ilp` 是最优 RVV family。

最新单次 production compare log 也能看到这个口径：

```text
pointnormal 262144: Std 58.3637 ms, RVV 50.0601 ms, speedup 1.17x
```

Std public baseline 本身比 diagnostic direct helper 更重，这会让 public Std/RVV speedup 与
diagnostic family B/A 出现方向差异。

### 3. 新增 production detail A/B 后，分歧复现为同边界负向

本阶段新增：

```text
collect_board_production_source_indices_detail_ba
```

该目标只运行 RVV binary，并在同一 production detail 边界内比较：

```text
staged-gather RVV baseline -> block-fused-abcd-ilp RVV candidate
```

输出：

```text
log/board/production_source_indices_detail_ba/analyze_rvv_ba.md
log/board/production_source_indices_detail_ba/ba_below_1_frequency.txt
log/board/production_source_indices_detail_ba/trace_summary.md
log/board/production_source_indices_detail_ba/checksum_validation.md
log/board/production_source_indices_detail_ba/collection_manifest.json
```

采集参数：

| 参数 | 值 |
| --- | --- |
| case-filter | `production-source-indices-detail-ba` |
| size | `65536,262144` |
| runs | `5` |
| iterations | `20` |
| warm-up iterations | `5` |
| CPU | `taskset -c 0` |
| governor/freq | `performance` / `1600000` |
| board temperature | before `40-41C`，after `43-44C` |
| checksum | 5 轮 checksum rows 存在且序列完全一致，final checksum `5232.840035` |

核心 B/A 结果：

| layer | point type | size | block-fused vs staged median |
| --- | --- | ---: | ---: |
| component | `pointnormal` | 65536 | `0.957x` |
| component | `pointnormal` | 262144 | `0.881x` |
| component | `pointxyz-to-pointnormal` | 65536 | `1.003x` |
| component | `pointxyz-to-pointnormal` | 262144 | `1.115x` |
| component | `pointxyz-to-pointxyzinormal` | 65536 | `0.989x` |
| component | `pointxyz-to-pointxyzinormal` | 262144 | `1.009x` |
| full | `pointnormal` | 65536 | `0.968x` |
| full | `pointnormal` | 262144 | `1.382x` |
| full | `pointxyz-to-pointnormal` | 65536 | `1.031x` |
| full | `pointxyz-to-pointnormal` | 262144 | `0.851x` |
| full | `pointxyz-to-pointxyzinormal` | 65536 | `1.018x` |
| full | `pointxyz-to-pointxyzinormal` | 262144 | `0.721x` |

这说明 production detail 边界下，block-fused 不是稳定优于 staged。特别是：

- `full pointxyz-to-pointxyzinormal 262144`：median `0.721x`，`5/5` avg below `1.0x`。
- `full pointxyz-to-pointnormal 262144`：median `0.851x`，`3/5` avg below `1.0x`。
- `component pointnormal 262144`：median `0.881x`，`5/5` avg below `1.0x`。
- `component pointnormal 65536`：median `0.957x`，`5/5` avg below `1.0x`。

因此 Phase 031 的 public Std/RVV 正向主要不能解释为“block-fused 比 staged 好”。更可能的解释是：
production scalar public baseline 较重，而 RVV fast path 即使不是最佳 RVV family，仍能快过 scalar public path。

## Production 端测试是否可信

Production correctness 是可信的，原因是：

| 检查 | 结果 |
| --- | --- |
| `run_test_source_indices_compare` | std `6 passed`，RVV `7 passed`。 |
| `run_test_production_direct_compare` | std `18 passed`，RVV `19 passed`。 |
| source-indexed invalid index gate | RVV helper 在 gather 前拒绝负数 / 越界 index，然后 public overload 保持 scalar fallback 边界。 |
| production detail A/B checksum | 5 轮 checksum 序列完全一致。 |

Production performance 也是可参考的，但要按问题分层：

- `production_source_indices_block_fused_abcd_ilp_probe/summary.md` 是 production public Std/RVV evidence。
- `production_source_indices_detail_ba/analyze_rvv_ba.md` 是 production detail RVV-vs-RVV A/B evidence。
- 二者都可信，但回答的问题不同，不能互相替代。

## Phase 030 是否意味着 test-rvv 测试代码有问题

当前没有证据说明 Phase 030 是 timer / warm-up / checksum 的实现 bug：

- Phase 030 repeated summary 已有 warm-up `5`。
- `run_case` / `run_case_trace` 的 warm-up 不计入 `Total Time`。
- measured checksum 会 sink 每轮 `fn()`，防止整段计算被消除。
- Phase 030 的 full estimate 已经把 normal-equation 和 matrix checksum 合并，减少 component/full sink 误读。

但 Phase 030 确实暴露了测试边界问题：

- 它只能作为 implementation-family diagnostic。
- 它不能预测 public production Std/RVV speedup。
- 后续如果看到 diagnostic negative，不应直接拒绝 production probe；应先补同边界 production detail A/B，确认是 family 本身负向还是 public scalar baseline 差异。

## Full-Cloud 对照

full-cloud production direct repeated summary：

```text
log/board/production_dispatch_fused_abcd_ilp/summary.md
```

结果：

| case | median | min |
| --- | ---: | ---: |
| `production-dispatch full-cloud pointnormal 262144` | `2.76x` | `2.73x` |
| `production-dispatch full-cloud pointxyz-to-pointnormal 262144` | `2.98x` | `2.95x` |
| `production-dispatch full-cloud pointxyz-to-pointxyzinormal 262144` | `3.00x` | `2.96x` |

这说明 full-cloud 在 production public Std/RVV 口径下非常稳定。默认 RVV path trace 也显示 5-run
checksum 序列一致，且有 production asm attribution。

但是 full-cloud 当前没有与 source-indexed Phase 032 完全同构的 production detail RVV-vs-RVV
同日志 A/B，因为 `production_default_fused_abcd_ilp` 当前只保留默认 fused path，不保留旧 block baseline pair。
因此可以说 full-cloud 也存在“production public 口径比 broad diagnostic 更干净、更强”的信号；不能说
full-cloud 已经用同一套 detail A/B 证明了相同根因。

## 当前决策

Phase 032 后的建议决策：

```text
source-indexed block-fused-abcd-ilp should not be promoted beyond bounded probe; public Std/RVV positive is credible, but staged-gather remains the safer production RVV family until same-boundary A/B turns positive
```

如果要降低维护风险，建议后续把 source-indexed production 默认顺序改回：

```text
staged-gather -> scalar
```

或把 block-fused 放在显式实验开关 / probe-only 分支，而不是默认优先 dispatch。

## 后续门禁

继续推进 source-indexed block-fused 前，至少需要：

- 20-run 或 50-run production detail A/B，保留 raw-dir 供长尾定位。
- source-indexed-specific asm attribution，比较 staged 与 block-fused 的 gather、vset、reduce、spill/reload。
- binary hash / build label 固化到 summary 或 manifest。
- 明确 production public Std/RVV speedup 与 RVV-vs-RVV family selection 是两类 evidence。

## Validation

本阶段实际运行：

| 命令 | 结果 |
| --- | --- |
| `make -C test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted collect_board_production_source_indices_detail_ba` | pass；生成 5-run production detail A/B。 |
| `make -C test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted run_test_source_indices_compare run_test_production_direct_compare` | pass；std/RVV correctness 全部通过。 |
| QEMU smoke for `run_bench_production_source_indices_detail_ba_compare` | pass；只用于编译和日志形状，不作为性能结论。 |
