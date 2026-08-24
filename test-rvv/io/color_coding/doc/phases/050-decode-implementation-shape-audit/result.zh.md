# Phase 050 Result: decode implementation-shape audit

## 结论

Phase 050 已完成。staged-store decode（分阶段暂存 decode，先连续写 scratch buffer 再标量写回 AoS 点云）通过 correctness（正确性）和 QEMU smoke（仿真器小型验证），但板卡 repeated benchmark（重复板卡性能测试）显示它比直接 `vsse32` AoS store 更差且更不稳定。

当前 EvidenceDecision（证据决策）维持 `partial-production-candidate`：`encodeAverageOfPoints`、`encodePoints` 的 average pass 和 `setDefaultColor` 仍是 PI1 / PI2 的窄生产候选；`decodePoints` 继续排除在生产补丁范围外。下一步若要修改 `io/include/pcl/compression/color_coding.h`，仍命中 `production_patch_authorization_required`，需要用户明确授权。

## 执行范围回填

| 计划动作 | 状态 | 证据 / 路径 | 结论 |
| --- | --- | --- | --- |
| RED gate：调用不存在的 staged-store helper | done | `src/test_color_coding.cpp` 中 production-shaped TEST 增加 staged-store 断言；早期 RVV build 因缺 helper 失败 | RED 阶段证明测试能捕获缺失实现。 |
| GREEN helper | done | `include/impl/color_coding_support.hpp` 新增 `decodePointsCandidatePointVectorStagedStore` | test-only helper 保持 reference byte stream 语义；production 未修改。 |
| bench labels | done | `src/bench_color_coding.cpp` 新增 `ps_decode_points_staged_leaf257` / `ps_decode_points_staged_leaf4096` | staged shape 已进入 board repeated summary。 |
| manifest metadata | done | `script/generate_color_coding_evidence_manifest.py` 标记 staged wrapper、case kind、asm boundary 和 phase050 summary metadata | Doctor 输入能区分 production-shaped diagnostic 与 implementation-shape diagnostic。 |
| QEMU / asm / board | done | 见下方命令和证据路径 | staged correctness 通过；板卡性能不支持继续。 |
| registry / freshness | done | `log/evidence_registry.json`，run label `board-color-coding-component-repeat-phase050` | phase 050 覆盖 phase 020 的同路径 summary，phase 020 数字降级为 historical evidence。 |

## 当前板卡证据

当前 summary path：

- `test-rvv/io/color_coding/log/board/component_repeat_5/summary.md`
- `test-rvv/io/color_coding/log/board/component_repeat_5/evidence_manifest.json`
- `test-rvv/io/color_coding/log/board/component_repeat_5/evidence_doctor.md`
- `test-rvv/io/color_coding/log/evidence_registry.json`

run label：`board-color-coding-component-repeat-phase050`。run count 为 5，iterations 为 20，warmup 为 3，device 为 `Milkv-Jupiter`。

| case | mean | median | min | max | decision bucket |
| --- | ---: | ---: | ---: | ---: | --- |
| `ps_decode_points_leaf257` | 1.0503x | 1.0499x | 1.0353x | 1.0650x | weak-positive diagnostic |
| `ps_decode_points_leaf4096` | 1.0241x | 1.0409x | 0.9116x | 1.0878x | unstable / warning |
| `ps_decode_points_staged_leaf257` | 0.9917x | 0.9918x | 0.9586x | 1.0195x | negative |
| `ps_decode_points_staged_leaf4096` | 0.9758x | 1.0120x | 0.8884x | 1.0458x | unstable / negative |

encode/default 当前仍保持正向：

| case | median |
| --- | ---: |
| `ps_encode_average_leaf257` | 2.5073x |
| `ps_encode_average_leaf4096` | 1.9590x |
| `ps_encode_points_leaf257` | 1.3050x |
| `ps_encode_points_leaf4096` | 1.1671x |
| `ps_set_default_color_4096` | 1.1655x |

## Evidence Doctor 回填

Evidence Doctor 结果为 `Errors=2, Warnings=11, Suggestions=6`。

| severity | case | 处理 |
| --- | --- | --- |
| Error | `ps_decode_points_staged_leaf257` 4/5 below 1 | staged-store helper 不进入 production candidate；该实现形态按 evidence rejected。 |
| Error | `ps_decode_points_staged_leaf4096` 2/5 below 1 | staged-store large case 仍不稳定，且 mean < 1；不允许把 near-threshold median 写成正向。 |
| Warning | `ps_decode_points_leaf4096` 1/5 below 1，min 0.9116x | 直接 AoS store decode 由 phase 020 的 Error 降级为 Warning，但仍是 weak / unstable diagnostic，不进入 PI2。 |
| Warning / Suggestion | decode near-threshold cases | decode 只保留为诊断线索；后续若继续，需要新的实现族或 profile，不重复 staged-store 假设。 |
| Warning | encode average group outlier、default large long tail | encode/default 候选按 case 单独报告；PI2 必须补 production direct evidence，不能 clean-adopt。 |

## diagnostic-to-production mismatch audit 回填

| question | answer |
| --- | --- |
| evidence role | `mixed_component_production_shaped_and_decode_shape_diagnostic`，其中 staged label 是 `production_shaped_implementation_shape_diagnostic`。 |
| A/B boundary | production-shaped helper，不是 public overload，也不是 production detail helper。 |
| 当前决策问题 | implementation-shape：staged scratch + scalar AoS write 是否比 direct `vsse32` AoS store 更值得继续。 |
| diagnostic 是否可外推到 production | no。当前证据只能说明 staged helper 在测试边界内不值得继续；不能直接证明完整 decompression public entry 的性能。 |
| comparison-boundary / baseline mismatch 风险 | yes。bench 不含 octree traversal、entropy coder、真实 leaf distribution 和 production dispatch。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | decode 不允许。staged candidate 触发 Doctor Error；direct candidate 仍 near-threshold 且不稳定。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | yes。当前没有 production patch，也没有 production direct A/B。 |

## 优化矩阵更新

| candidate family | correctness | board evidence | Evidence Doctor | decision | next action |
| --- | --- | --- | --- | --- | --- |
| direct production-shaped decode | pass | `ps_decode_points_leaf4096` median 1.0409x，但 min 0.9116x | Warning + near-threshold Suggestions | rejected/deferred for production | keep scalar unless a new decode family is explicitly chosen later。 |
| staged-store production-shaped decode | pass | leaf257 median 0.9918x；leaf4096 mean 0.9758x / min 0.8884x | two staged-specific Errors | rejected with evidence | do not repeat staged-store probe without new root-cause evidence。 |
| encode average / encode points / default | pass | production-shaped medians remain positive | Warnings require production direct follow-up | partial-production-candidate | wait for PI2 production patch authorization。 |

## 命令和验证

已执行并通过：

```bash
make -C test-rvv/io/color_coding run_test_rvv
make -C test-rvv/io/color_coding run_test_compare
make -C test-rvv/io/color_coding run_bench_rvv BENCH_ARGS="--case-filter ps_decode_points_staged_leaf4096 --iterations 2 --warmup-iterations 1 --batch-repeats 2"
make -C test-rvv/io/color_coding dump_bench_rvv
make -C test-rvv/io/color_coding run_board_color_coding_repeated
```

本轮 metadata 修正后未重新跑板卡，只用已有 `component_repeat_5/run*/` raw logs 再生成 summary / manifest / Doctor / registry。后续 closeout 验证将再次运行：

```bash
make -C test-rvv/io/color_coding check_evidence_freshness
make -C test-rvv/io/color_coding run_test_compare
git diff --check -- test-rvv/io/color_coding tmp/rvv-work-logs/io/color_coding
```

## 阶段反思

staged scratch 写入没有降低 decode 风险，反而增加了 scratch resize、连续写和标量 AoS 回写成本。该结果说明 phase 020 的 decode 问题不能简单归因于 `vsse32` 跨步写；更可能与整体 decode 数据流、内存写回、短收益边界或测量波动共同有关。没有 profile（性能剖析）或新的实现族前，不建议继续把 decode 放进生产接入范围。

新增 roadmap 事实：

- `decode staged-store shape`：rejected with evidence。
- `decode direct AoS store`：weak / unstable diagnostic，keep scalar。
- `PI2 encode/default production patch`：仍是默认恢复入口，但需要用户明确授权。

## Continue / Stop Decision

`continue_stop_decision`: `turn_stop_deferred with stop_condition_hit=production_patch_authorization_required`。

`stop_condition_hit`: 继续下一步会修改 `io/include/pcl/compression/color_coding.h`，而本轮没有明确 PI2 production patch 授权。当前 topic-local 测试资产、phase 050 证据、Doctor、registry 和文档 closeout 已闭合；板卡可用不是阻塞项。

`next_phase_default`: 若用户授权 PI2，按 phase 030 计划只修改 encode/default 窄范围：`encodeAverageOfPoints`、`encodePoints` average pass、`setDefaultColor`；`decodePoints` 保持标量。
