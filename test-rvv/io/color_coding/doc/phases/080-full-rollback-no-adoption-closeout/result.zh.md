# Phase 080 Result: full rollback no-adoption closeout

## 结论

Phase 080 已完成 full rollback（完整回滚）：`io/include/pcl/compression/color_coding.h` 中剩余的 `setDefaultColor` RVV production 分流已移除。当前 production `ColorCoding` 公开入口全部回到标量路径：

- `encodeAverageOfPoints`：标量。
- `encodePoints`：标量。
- `decodePoints`：标量。
- `setDefaultColor`：标量。

当前 topic 结论为 `no production RVV adopted`。不创建 `doc-rvv/io/color_coding-RVV.zh.md`。

## 执行范围回填

| 计划动作 | 状态 | 证据 / 路径 | 结论 |
| --- | --- | --- | --- |
| 移除 production default RVV | done | `io/include/pcl/compression/color_coding.h` | 删除 `__RVV10__` include / gate、`setDefaultColorRVV`、default RVV hook；公开入口直接调用标量 helper。 |
| 更新 production direct tests | done | `src/test_color_coding.cpp` | 删除 `PCL_COLOR_CODING_RVV_TEST_HOOK` 和 hook 断言；保留真实 public method 语义测试。 |
| 更新 production bench | done | `src/bench_color_coding.cpp` | `--case-filter production` 不再选择任何 current production case。 |
| 保护 historical evidence | done | `Makefile` | `run_board_color_coding_production_repeated` 现在显式 skip，避免完整回滚后覆盖 Phase 070 no-adoption evidence。 |
| 本地验证 | done | `run_test_compare`、production filter smoke、`check_evidence_freshness`、`diff --check` | 正确性通过，production filter 无 current case，registry fresh，diff clean。 |

## 当前 production patch 状态

`color_coding.h` 仍保留 Phase 060 为了生产接入可审查性抽出的标量 helper：

- `encodeAverageOfPointsStd`
- `encodePointsStd`
- `setDefaultColorStd`

这些 helper 只服务公开入口的标量路径；当前没有 RVV dispatch（分流）、RVV intrinsic（内建向量指令调用）或 RVV test hook。

## 证据链

Phase 080 不生成新板卡性能证据，因为完整回滚后没有 production RVV path 可比较。no-adoption decision（不采纳决策）仍由 Phase 070 的 production-public evidence 支撑：

- `test-rvv/io/color_coding/log/board/production_repeat_5/summary.md`
- `test-rvv/io/color_coding/log/board/production_repeat_5/evidence_manifest.json`
- `test-rvv/io/color_coding/log/board/production_repeat_5/evidence_doctor.md`
- `test-rvv/io/color_coding/log/evidence_registry.json`

Phase 070 default-only 结果：

| case | mean | median | min | max | decision |
| --- | ---: | ---: | ---: | ---: | --- |
| `prod_set_default_color_4096` | 1.0054x | 1.0035x | 0.9945x | 1.0252x | neutral / unstable, Error |

Evidence Doctor：`Errors=1, Warnings=0, Suggestions=1`。Error 为 `prod_set_default_color_4096` 2/5 runs below 1。

## 命令和验证

已执行：

```bash
make -C test-rvv/io/color_coding run_test_compare
make -C test-rvv/io/color_coding run_bench_rvv BENCH_ARGS="--case-filter production --iterations 1 --warmup-iterations 1 --batch-repeats 1"
make -C test-rvv/io/color_coding run_bench_std BENCH_ARGS="--case-filter production --iterations 1 --warmup-iterations 1 --batch-repeats 1"
make -C test-rvv/io/color_coding check_evidence_freshness
git diff --check -- io/include/pcl/compression/color_coding.h test-rvv/io/color_coding tmp/rvv-work-logs/io/color_coding doc-rvv/library-screening/io/io-retained-candidate-rescreen.zh.md doc-rvv/io/color_coding-RVV.zh.md
```

验证结果：

- Std/RVV correctness：8/8 tests passed on both builds。
- production filter smoke：只输出 dataset / iterations / batch repeats，无 `prod_*` current case。
- evidence registry：fresh。
- whitespace：`diff --check` clean。

## 阶段反思

现有实现族已经完整闭合：

- indexed gather + vector reduction：production public evidence 负向或弱 / 不稳定，已回滚。
- average-pass-only encode：被 diff byte stream 和 public method 边界吞掉，已回滚。
- default color vector store：default-only 复跑 near-zero 且触发 Doctor Error，已回滚。
- decode direct / staged-store：phase 050 已拒绝或保持 weak / unstable，未进入 production。

继续推进需要新的 profile（性能剖析）、新的 implementation family（实现族）或完整 compression public entry（公开压缩入口）证据；这些都已经超出当前证据支持的自动推进范围。当前不建议继续在本 topic 上探索。

## Continue / Stop Decision

`continue_stop_decision`: `turn_stop_deferred with stop_condition_hit`。

`stop_condition_hit`: 当前 production 源码已完整回滚，roadmap 和 optimization matrix 中没有当前授权范围内、未阻塞且建议继续的高优先级 RVV candidate。继续需要新的外部 profile / 用户明确要求的新实现族。

`next_phase_default`: none。若未来重开，默认从新 profile 或新实现族的独立 phase plan 开始，而不是恢复现有 production patch。
