# Phase 032 Plan：source-indexed production / diagnostic boundary root-cause

## 背景

用户在 2026-08-13 追问 Phase 030 与 Phase 031 的分歧：

- Phase 030 `source_indexed_family_repeated` 中 `block-fused-abcd-ilp pointnormal 262144`
  full estimate median 为 `0.90x`，`4/5` 低于 `1.0x`。
- Phase 031 production source-indexed public overload probe 中 6 个代表 case median 都为正向，
  范围 `1.54x` 到 `1.71x`。

本阶段目标不是继续扩大 production 接入，而是解释这两个结果为什么能同时成立，并回答：

- production 端测试是否可信。
- Phase 030 是否说明 test-rvv 测试代码有 bug。
- full-cloud 是否也存在 production direct 比 test-rvv diagnostic 更好的现象。

## 核心假设

Phase 031 的 positive 不等价于 `block-fused-abcd-ilp` 打赢 source-indexed staged RVV。它只说明：

```text
production public scalar path  ->  production public RVV path
```

这组 Std/RVV speedup 为正向。

Phase 030 测的是：

```text
test-rvv diagnostic std/helper baseline  ->  test-rvv diagnostic RVV candidate
```

因此两个阶段的基线、入口和可解释问题不同。若要隔离根因，必须补一条同一 production RVV binary 内的
detail A/B：

```text
production source-indexed staged RVV  ->  production source-indexed block-fused RVV
```

## 工作项

| item | 动作 | 通过条件 |
| --- | --- | --- |
| P1 production detail A/B harness | 增加 topic-local bench wrapper，直接调用 production detail staged helper 和 block-fused helper。 | 同一 RVV log 能输出 component no-solve 和 full estimate 的 staged / block-fused pair。 |
| P2 analyzer 配对 | 扩展 `analyze_teptplw_rvv_ba.py`，把 `production-source-indices-detail` 的 baseline 设为 `staged-gather`。 | QEMU smoke 输出非空 B/A 表。 |
| P3 board repeated detail A/B | 新增并运行 `collect_board_production_source_indices_detail_ba`。 | 5 runs、20 iterations、5 warm-up、taskset CPU0；输出 `analyze_rvv_ba.md`、`ba_below_1_frequency.txt`、`trace_summary.md`、`checksum_validation.md`。 |
| P4 correctness 复核 | 重跑 source-indexed 和 production direct QEMU gtest。 | std/RVV 均通过。 |
| P5 root-cause closeout | 写明 030/031 分歧的真正含义、full-cloud 对照和后续门禁。 | Phase 032 result、phase README 和 handoff 更新。 |

## 证据路径

新 detail A/B 输出目录：

```text
test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted/log/board/production_source_indices_detail_ba/
```

核心摘要文件：

```text
analyze_rvv_ba.md
ba_below_1_frequency.txt
checksum_validation.md
trace_summary.md
collection_manifest.json
```

raw `run*.log` 和 `board_env_*.log` 仍按 summary-only 策略默认不提交。

## Stop 条件

若 production detail A/B 显示 block-fused 对 staged 仍有明显负向或 mixed signal，则 Phase 032 直接关闭为：

```text
root cause identified as comparison-boundary / baseline mismatch; Phase 031 public speedup is credible but does not prove block-fused beats staged RVV
```

若 detail A/B 也全面正向，再继续检查 Phase 030 test_support helper 和 production helper 的代码差异、asm 和
计时 sink。
