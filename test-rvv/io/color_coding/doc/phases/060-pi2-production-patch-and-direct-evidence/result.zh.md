# Phase 060 Result: PI2 production patch and direct evidence

## 结论

Phase 060 已完成到 PI5 user checkpoint（用户检查点）。本阶段把 `io/include/pcl/compression/color_coding.h` 接入了窄范围 RVV production patch（生产补丁），并补齐 production direct correctness（真实生产路径正确性）、fallback（回退路径）、QEMU smoke（仿真器日志形状）、asm attribution（反汇编归属）和 5-run board repeated evidence（板卡重复性能证据）。

当前结果不支持完整采纳该 production patch：

- `encodeAverageOfPoints`：真实 production public evidence 为负向，`leaf257` 和 `leaf4096` 均触发 Evidence Doctor Error，不建议保留 RVV 分流。
- `encodePoints` average pass：只有弱正向，`leaf4096` 有 1/5 退化且两个规模都是 near-threshold，不建议作为默认生产采纳，除非后续有新的实现形态或更强证据。
- `setDefaultColor`：真实 production public evidence 稳定正向，median 1.1009x，min 1.0635x，可作为“只保留 default color RVV”的候选。
- `decodePoints`：保持标量，未修改。

PI5 规则要求保留当前 patch 并等待用户确认。建议下一步由用户确认是否执行“部分回滚”：移除 `encodeAverageOfPoints` 和 `encodePoints` 的 production RVV 分流，只保留 `setDefaultColor` RVV；确认前不自行回滚。

## 执行范围回填

| 计划动作 | 状态 | 证据 / 路径 | 结论 |
| --- | --- | --- | --- |
| RED production direct test | done | `src/test_color_coding.cpp` 先因缺少 `resetRvvTestHook` / `rvvTestHook*` 失败 | RED 命中生产分流缺失。 |
| production patch | done | `io/include/pcl/compression/color_coding.h` | exact `pcl::PointXYZRGBA` gate；小规模、非 exact 点型和非 RVV 构建 fallback；`decodePoints` 未修改。 |
| fallback tests | done | `ColorCodingProductionDirect.RvvFallbackGatesKeepScalarSemantics` | 小规模和 `PointXYZRGB` fallback 保持标量语义，RVV hook 不命中。 |
| production direct bench labels | done | `prod_encode_average_leaf257/4096`、`prod_encode_points_leaf257/4096`、`prod_set_default_color_4096` | labels 调用真实 `ColorCoding<pcl::PointXYZRGBA>` public methods。 |
| QEMU correctness / smoke | done | `run_test_compare`，`run_bench_rvv --case-filter production`，`run_bench_std --case-filter production` | 正确性通过；QEMU timing 不作为性能结论。 |
| asm attribution | done | `build/asm/riscv/bench_color_coding_rvv.asm` | 可见 `vluxei32`、`vredsum`、`vsse32`。 |
| board repeated + Doctor + registry | done | `log/board/production_repeat_5/*`，`log/evidence_registry.json` | production direct evidence 生成并登记。 |

## Production patch 摘要

生产源码改动只在 `io/include/pcl/compression/color_coding.h`：

- 增加 `__RVV10__` 下的 `encodeAverageOfPointsRVV`、`encodePointsRVV`、`setDefaultColorRVV`。
- 将原标量主体抽成 `encodeAverageOfPointsStd`、`encodePointsStd`、`setDefaultColorStd`。
- RVV gate：exact `pcl::PointXYZRGBA`、`rgba_offset_arg == offsetof(pcl::PointXYZRGBA, rgba)`、32-bit index byte offset 可表达、规模阈值足够。
- `encodePointsRVV` 只替换第一遍 average pass；diff byte `push_back` 保留标量。
- `decodePoints` 保持原标量实现。
- `PCL_COLOR_CODING_RVV_TEST_HOOK` 仅用于 topic 测试，不属于公开 API。

## Production direct board evidence

当前 summary：

- `test-rvv/io/color_coding/log/board/production_repeat_5/summary.md`
- `test-rvv/io/color_coding/log/board/production_repeat_5/evidence_manifest.json`
- `test-rvv/io/color_coding/log/board/production_repeat_5/evidence_doctor.md`
- `test-rvv/io/color_coding/log/evidence_registry.json`

run label：`board-color-coding-production-repeat-phase060`。run count 为 5，iterations 为 20，warmup 为 3，device 为 `Milkv-Jupiter`。

| case | mean | median | min | max | decision bucket |
| --- | ---: | ---: | ---: | ---: | --- |
| `prod_encode_average_leaf257` | 0.9826x | 0.9862x | 0.9483x | 1.0163x | negative / Error |
| `prod_encode_average_leaf4096` | 0.8613x | 0.9187x | 0.6834x | 0.9386x | negative / Error |
| `prod_encode_points_leaf257` | 1.0527x | 1.0323x | 1.0029x | 1.1768x | weak-positive / near-threshold |
| `prod_encode_points_leaf4096` | 1.0243x | 1.0290x | 0.9397x | 1.0819x | weak-positive / unstable warning |
| `prod_set_default_color_4096` | 1.1097x | 1.1009x | 1.0635x | 1.1814x | positive |

## Evidence Doctor 回填

Evidence Doctor 结果为 `Errors=2, Warnings=6, Suggestions=2`。

| severity | case | 处理 |
| --- | --- | --- |
| Error | `prod_encode_average_leaf257` 4/5 below 1 | 不支持保留 `encodeAverageOfPoints` RVV production 分流。 |
| Error | `prod_encode_average_leaf4096` 5/5 below 1 | 大 leaf 真实 production path 明显慢于标量，应回滚该分流，等待用户确认。 |
| Warning | `prod_encode_points_leaf4096` 1/5 below 1，min 0.9397x | 只能写弱正向 / 不稳定，不建议默认采纳。 |
| Warning | `prod_encode_points_leaf257/4096` long-tail 或 near-threshold | 如果保留，需要更强 evidence 或新实现形态；当前不建议采纳。 |
| Warning | `prod_encode_average_*` group outlier / long-tail | 不能把其它 case 的收益外推到 encode average。 |
| Suggestion | `prod_encode_points_leaf257/4096` near-threshold | 后续可做更轻量 dispatch 或同边界新候选，但当前 PI5 不 clean-adopt。 |

## Diagnostic-to-production mismatch audit 回填

| question | answer |
| --- | --- |
| evidence role | `production_public`；每个 `prod_*` comparison 的 `case_kind` 为 `production_direct`。 |
| A/B boundary | `public_overload`，真实 `ColorCoding<pcl::PointXYZRGBA>` public methods。 |
| 当前决策问题 | 接入后的 RVV production path 是否比当前 scalar production path 更值得保留。 |
| diagnostic 是否可外推到 production | phase 050 的 production-shaped positive 不能外推。phase 060 真实 production public data 覆盖该决策。 |
| comparison-boundary / baseline mismatch 风险 | 当前 Std/RVV 使用同一 bench wrapper 和同名 public method label；仍未覆盖完整 `OctreePointCloudCompression`、entropy coder 和真实 leaf distribution。 |
| clean adoption 是否需要 user confirmation | yes。即使 default color positive，也必须等 PI5 用户确认后才能写 adopted production behavior。 |

## 优化矩阵更新

| candidate family | correctness | board evidence | Evidence Doctor | decision | next action |
| --- | --- | --- | --- | --- | --- |
| production encode average RVV | pass | negative：median 0.9862x / 0.9187x | Errors=2 | reject / pending rollback confirmation | 用户确认后回滚该 production 分流。 |
| production encode points average RVV | pass | weak-positive：median 1.0323x / 1.0290x，large min 0.9397x | Warnings + Suggestions | not recommended for adoption | 用户确认后回滚；或另开新实现族。 |
| production default color RVV | pass | positive：median 1.1009x，min 1.0635x | no case-specific Error | production candidate pending user confirmation | 用户确认后可只保留该分流。 |
| decode scalar | existing pass | not_applicable | not_applicable | keep scalar | no action。 |

## 命令和验证

已执行：

```bash
make -C test-rvv/io/color_coding run_test_rvv
make -C test-rvv/io/color_coding run_test_compare
make -C test-rvv/io/color_coding run_bench_rvv BENCH_ARGS="--case-filter production --iterations 1 --warmup-iterations 1 --batch-repeats 1"
make -C test-rvv/io/color_coding run_bench_std BENCH_ARGS="--case-filter production --iterations 1 --warmup-iterations 1 --batch-repeats 1"
make -C test-rvv/io/color_coding dump_bench_rvv
make -C test-rvv/io/color_coding run_board_color_coding_production_repeated
```

验证结果：

- Std/RVV correctness：8/8 tests passed on both builds。
- production bench smoke：5 个 `prod_*` labels 输出 checksum。
- asm：bench RVV asm 中可见 `vluxei32`、`vredsum`、`vsse32`。
- board repeated：5-run production summary 生成。
- Evidence Doctor：Errors=2, Warnings=6, Suggestions=2。

## 阶段反思

phase 050 的 production-shaped diagnostic 高估了真实 production public收益。真实 `ColorCoding` public method 里，`encodeAverageOfPoints` 的调用/模板/容器边界、byte offset gather、规约和平均写出没有形成净收益；`encodePoints` 虽然复用了 average pass，但总体仍被 diff byte stream 标量 `push_back` 主导，只剩弱正向。`setDefaultColor` 的连续写场景更接近 RVV store 的优势，因此成为唯一稳定正向 production candidate。

新增 roadmap 事实：

- `production encode average RVV`：rejected by production public evidence。
- `production encode points average RVV`：weak / near-threshold，不建议采纳。
- `production default color RVV`：positive candidate，等待 PI5 用户确认。
- `partial rollback after PI5`：建议用户确认后执行，范围为移除 encode average / encode points RVV，只保留 default color RVV。

## Continue / Stop Decision

`continue_stop_decision`: `pi5_user_checkpoint_required`。

`stop_condition_hit`: 已进入 production integration loop 并获得 post-patch board evidence。AGENTS.md 规定 PI5 是对称用户检查点；当前不能自行采纳 default，也不能自行回滚 encode patch。

`next_phase_default`: 等待用户确认：

1. 推荐选项：授权部分回滚 encode RVV 分流，只保留 `setDefaultColor` RVV，并重跑 production direct tests / board。
2. 或授权完整回滚 production patch，保留本阶段为 negative / weak production probe evidence。
3. 或要求继续探索新的 encode 实现族；当前不建议在现有 patch 上 clean-adopt。
