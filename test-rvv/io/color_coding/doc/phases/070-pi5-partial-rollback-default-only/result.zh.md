# Phase 070 Result: PI5 partial rollback default-only

## 结论

Phase 070 已按用户授权完成部分回滚：`encodeAverageOfPoints` 和 `encodePoints` 的 production RVV 分流已移除，公开入口回到标量路径；`decodePoints` 继续保持标量；`setDefaultColor` RVV 暂时保留并重跑 default-only production-public evidence（真实公开入口生产证据）。

新证据不支持把 `setDefaultColor` RVV 写成 adopted production behavior（已采用生产行为）：

- `prod_set_default_color_4096` 5-run board median 只有 `1.0035x`，min `0.9945x`，mean `1.0054x`。
- Evidence Doctor 为 `Errors=1, Warnings=0, Suggestions=1`，Error 是 `2/5` runs 低于 1。
- 按本阶段计划的 positive 规则，default-only 不达 `median >= 1.05x` 且 `min >= 1.0x`，因此降级为 neutral / unstable，不建议保留。

当前 topic 命中 stop condition（停止条件）：现有生产接入候选已经没有值得继续优化和采纳的方向。因为用户只授权了部分回滚，本轮未自行完整回滚 `setDefaultColor` RVV；建议下一步由用户确认是否完整回滚剩余 default RVV 分流。

## 执行范围回填

| 计划动作 | 状态 | 证据 / 路径 | 结论 |
| --- | --- | --- | --- |
| 部分回滚 production encode RVV | done | `io/include/pcl/compression/color_coding.h` | `encodeAverageOfPoints` / `encodePoints` 直接调用 Std helper；删除 encode RVV helpers 和 encode test hook。 |
| 保留 default RVV 并重测 | done | `setDefaultColorRVV` 保留 exact `PointXYZRGBA` + RGBA offset + point_count gate | 可正确运行，但新板卡证据不支持采纳。 |
| 更新 production direct tests | done | `src/test_color_coding.cpp` | RVV 构建只要求 default hook 命中；encode 只断言语义。 |
| 更新 production bench filter | done | `src/bench_color_coding.cpp` | `--case-filter production` 只输出 `prod_set_default_color_4096`。 |
| 更新 registry doc refs | done | `Makefile` | production run label 变为 `board-color-coding-production-repeat-phase070`，doc-ref 指向本结果文件。 |
| 本地 / QEMU 验证 | done | `run_test_compare`、production smoke、`dump_bench_rvv` | 正确性通过，QEMU production filter 只输出 default label，asm 中有 `vsse32`。 |
| 板卡验证 | done | `log/board/production_repeat_5/*` | default-only 5-run summary + manifest + Doctor + registry fresh。 |

## Production patch 当前状态

当前 production diff 只剩：

- 原标量主体仍拆为 `encodeAverageOfPointsStd`、`encodePointsStd`、`setDefaultColorStd`。
- `setDefaultColor` 在 `__RVV10__` 下尝试 `setDefaultColorRVV`，失败时回退 `setDefaultColorStd`。
- `setDefaultColorRVV` 只覆盖 exact `pcl::PointXYZRGBA`、标准 RGBA offset、`pointCount >= 64`。
- `PCL_COLOR_CODING_RVV_TEST_HOOK` 只暴露 default hook。

注意：虽然 production diff 仍有 default RVV，但本阶段不建议把它作为最终保留项。

## Production direct board evidence

当前 summary：

- `test-rvv/io/color_coding/log/board/production_repeat_5/summary.md`
- `test-rvv/io/color_coding/log/board/production_repeat_5/evidence_manifest.json`
- `test-rvv/io/color_coding/log/board/production_repeat_5/evidence_doctor.md`
- `test-rvv/io/color_coding/log/evidence_registry.json`

run label：`board-color-coding-production-repeat-phase070`。run count 为 5，iterations 为 20，warmup 为 3，device 为 `Milkv-Jupiter`。

| case | mean | median | min | max | decision bucket |
| --- | ---: | ---: | ---: | ---: | --- |
| `prod_set_default_color_4096` | 1.0054x | 1.0035x | 0.9945x | 1.0252x | neutral / unstable, Error |

## Evidence Doctor 回填

Evidence Doctor 结果为 `Errors=1, Warnings=0, Suggestions=1`。

| severity | case | 处理 |
| --- | --- | --- |
| Error | `prod_set_default_color_4096` 2/5 below 1 | 不能写成稳定加速；default RVV production 分流不建议保留。 |
| Suggestion | `prod_set_default_color_4096` near-threshold | median 仅 1.0035x，收益可被测量波动反转；不建议为了弱收益承担 production patch。 |

## Diagnostic-to-production mismatch audit 回填

| question | answer |
| --- | --- |
| evidence role | `production_public`；只包含 `prod_set_default_color_4096`。 |
| A/B boundary | `public_overload`，真实 `ColorCoding<pcl::PointXYZRGBA>::setDefaultColor` public method。 |
| 当前决策问题 | default-only RVV production path 是否值得保留。 |
| diagnostic 是否可外推到 production | 不使用 diagnostic 外推；Phase 070 重跑了真实 production-public evidence。 |
| comparison-boundary / baseline mismatch 风险 | 不覆盖完整 `OctreePointCloudCompression`、entropy coder 和真实 leaf distribution；但足以回答当前 color coder public method 分流是否有稳定收益。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 不需要；当前结果已因 RVV-vs-scalar 不达标而不建议采纳。 |

## 优化矩阵更新

| candidate family | correctness | board evidence | Evidence Doctor | decision | next action |
| --- | --- | --- | --- | --- | --- |
| production encode average RVV | pass after scalar rollback | historical phase 060 negative | historical Errors=2 | rejected and rolled back from current patch | no further action without new implementation family / profile。 |
| production encode points average RVV | pass after scalar rollback | historical phase 060 weak / unstable | historical Warning + Suggestions | rejected and rolled back from current patch | no further action without new implementation family / profile。 |
| production default color RVV | pass | phase 070 neutral / unstable：median 1.0035x，min 0.9945x | Error=1 | not recommended for adoption | superseded by phase 080 full rollback。 |
| decode scalar | existing pass | not_applicable | not_applicable | keep scalar | no action。 |

## 命令和验证

已执行：

```bash
make -C test-rvv/io/color_coding run_test_compare
make -C test-rvv/io/color_coding run_bench_rvv BENCH_ARGS="--case-filter production --iterations 1 --warmup-iterations 1 --batch-repeats 1"
make -C test-rvv/io/color_coding run_bench_std BENCH_ARGS="--case-filter production --iterations 1 --warmup-iterations 1 --batch-repeats 1"
make -C test-rvv/io/color_coding dump_bench_rvv
make -C test-rvv/io/color_coding run_board_color_coding_production_repeated
make -C test-rvv/io/color_coding check_evidence_freshness
```

验证结果：

- Std/RVV correctness：8/8 tests passed on both builds。
- production bench smoke：只输出 `prod_set_default_color_4096`，Std/RVV checksum 一致。
- asm：bench RVV asm 中可见 `vsse32`；仍可见 diagnostic candidate 的 `vluxei32` / `vredsum`，但它们不属于当前 production filter。
- board repeated：5-run production summary 生成。
- Evidence Doctor：Errors=1, Warnings=0, Suggestions=1。
- evidence registry：fresh。

## 阶段反思

Phase 060 中 `setDefaultColor` 与 encode labels 同批运行时看似稳定正向，但 Phase 070 default-only 复跑后收益回到 near-zero，并出现 2/5 below 1。可能原因包括板卡波动、二进制布局变化、bench 批次差异或 default helper 本身太短，任何一种都足以让 production 结论降级：当前证据不能支撑长期保留 default RVV。

新增 roadmap 事实：

- `production default color RVV`：Phase 070 default-only rejected / not recommended。
- `production encode average RVV`：已从当前 patch 回滚；历史负向证据保留。
- `production encode points average-pass RVV`：已从当前 patch 回滚；历史弱正 / 不稳定证据保留。
- `doc-rvv/io/color_coding-RVV.zh.md`：不创建，因为没有 adopted production behavior。

## Continue / Stop Decision

`continue_stop_decision`: `turn_stop_deferred with stop_condition_hit`。

`stop_condition_hit`: 当前 production-public evidence 不支持任何已实现 RVV 分流 clean adoption；继续探索需要新的实现族、profile 或扩大到完整 compression public entry，当前 evidence 不建议继续推进。剩余 default RVV 的完整回滚属于 PI5 后生产决策，需要用户确认。

`next_phase_default`: 等待用户确认：

1. 推荐选项：授权完整回滚剩余 `setDefaultColor` RVV 分流，保留 Phase 070 作为 no-adoption closeout evidence。
2. 或明确要求继续探索新实现族 / 新 profile；当前不建议在现有 color_coding topic 内继续自动推进。
