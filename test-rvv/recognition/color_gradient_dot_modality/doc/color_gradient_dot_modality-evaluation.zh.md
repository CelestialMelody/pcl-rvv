# color_gradient_dot_modality 函数级评估

## S2 函数级评估

目标源码是 `recognition/include/pcl/recognition/color_gradient_dot_modality.h`。公开入口
`ColorGradientDOTModality<PointInT>::processInputData()` 从 organized RGB 输入点云开始，先调用
`computeMaxColorGradients()` 生成逐像素 `GradientXY`，再调用 `computeDominantQuantizedGradients()`
把每个 `bin_size_ x bin_size_` 区域里 magnitude 最大的梯度量化成 dominant map（主方向图）。

标量路径的关键语义如下：

- `computeMaxColorGradients()` 对 `(row, col)` 与 `(row, col + 2)`、`(row + 2, col)` 做 RGB 三通道差分，
  选择平方幅值最大的通道；若红色不是严格大于绿色和蓝色，绿色只在严格大于蓝色时胜出，否则蓝色胜出。
- 梯度写入 `color_gradients_(col + 1, row + 1)`，边框和未写位置保持默认值。
- `computeDominantQuantizedGradients()` 对每个 output bin 只保留该 bin 内 magnitude 最大的梯度；
  阈值使用 `gradient_magnitude_threshold_`，量化用 7 个方向 bin，空 bin 写 `1 << 7`。
- `computeInvariantQuantizedMap()` 使用 region / mask 形态，并会临时把 selected gradients 的 magnitude
  改成 `-1` 再恢复；本阶段不覆盖这条状态恢复路径。

## 初步判断

当前判断为 adopted production behavior（已采用生产行为）。Phase 000 的 production-shaped diagnostic
证明 gradient-dominant 候选值得接入；Phase 010 已把 `computeMaxColorGradientsRVV()` 接到
`processInputData()`，并用 production direct（真实生产路径）板卡证据确认收益。`computeInvariantQuantizedMap()`
和 DOTMOD template matching（模板匹配）仍不属于本次已采用范围。

## doc_suite_role_inventory

| role | status | path |
| --- | --- | --- |
| topic_navigation | `standalone:README.zh.md` | `README.zh.md` |
| testing_overview | `standalone:doc/testing-overview.zh.md` | `doc/testing-overview.zh.md` |
| correctness_tests | `standalone:doc/correctness-tests.zh.md` | `doc/correctness-tests.zh.md` |
| benchmark_and_evidence | `standalone:doc/benchmark-and-evidence.zh.md` | `doc/benchmark-and-evidence.zh.md` |
| optimization_evidence | `standalone:doc/optimization-evidence.zh.md` | `doc/optimization-evidence.zh.md` |
| optimization_roadmap | `standalone:doc/optimization-roadmap.zh.md` | `doc/optimization-roadmap.zh.md` |
| test_support_code_map | `standalone:doc/test-support-code-map.zh.md` | `doc/test-support-code-map.zh.md` |
| phase_index | `standalone:doc/phases/README.zh.md` | `doc/phases/README.zh.md` |
| phase_plan / phase_result / optimization_matrix | `standalone:doc/phases/*` | `doc/phases/*` |
| production_topic_doc | `standalone:../../../doc-rvv/recognition/color_gradient_dot_modality-RVV.zh.md` | `../../../doc-rvv/recognition/color_gradient_dot_modality-RVV.zh.md` |

## target granularity audit

| target 类别 | current shape scan | decision | 说明 |
| --- | --- | --- | --- |
| correctness aggregate | `run_test_compare` | adopted | 汇总 Std/RVV gtest，对拍公开入口。 |
| bench diagnostic aliases | `run_bench_rvv` + case-filter | adopted | case 分开 helper-level 与 production direct。 |
| QEMU smoke aliases | `run_qemu_smoke` | adopted | 只证明正确性和日志形状。 |
| board repeated aliases | `board_repeated` | adopted | 当前 production direct 的主要板卡入口。 |
| doctor / registry aliases | `evidence_manifest_repeated`、`evidence_doctor_repeated`、`record_evidence_state_repeated`、`check_evidence_freshness` | adopted | 让 summary / manifest / doctor / registry 对齐。 |
| correctness aliases / historical probe | 无额外 topic-local 入口 | not_applicable with evidence | 当前 topic 已收口，不再保留旧 probe。 |

## Traceability Map

| 符号 / 文件 | 层级 | 作用 | 调用者 / 上游入口 | 被调用者 / 下游消费者 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- | --- | --- |
| `ColorGradientDOTModality::processInputData` | production public entry | DOTMOD color-gradient 预处理入口 | DOTMOD template creation / detection 输入准备 | `computeMaxColorGradients` / `computeMaxColorGradientsRVV`、`computeDominantQuantizedGradients` | adopted production RVV boundary | `recognition/include/pcl/recognition/color_gradient_dot_modality.h` |
| `computeMaxColorGradients` | production helper | RGB 差分、最大通道选择、magnitude / angle 写入 | `processInputData` | dominant bin map | scalar path source of truth | `recognition/include/pcl/recognition/color_gradient_dot_modality.h` |
| `computeMaxColorGradientsRVV` | production helper | RVV 跨步读取 RGB 字段并计算梯度 magnitude / angle | `processInputData` | dominant bin map | adopted production RVV path | `recognition/include/pcl/recognition/color_gradient_dot_modality.h` |
| `computeDominantQuantizedGradients` | production helper | 每个 bin 选择最大梯度并写 one-hot / empty bit | `processInputData` | DOTMOD dominant map | scalar path source of truth | `recognition/include/pcl/recognition/color_gradient_dot_modality.h` |
| `computeDominantMapScalar` | diagnostic reference | 复刻 `processInputData()` 预处理链 | `test_cgdm`、`bench_cgdm` | correctness oracle | correctness reference | `test-rvv/recognition/color_gradient_dot_modality/include/impl/cgdm_color_gradient.hpp` |
| `computeDominantMapCandidate` | candidate helper | RVV build 下尝试预处理链 candidate | `test_cgdm`、`bench_cgdm` | output checksum / gtest | production-shaped diagnostic | `test-rvv/recognition/color_gradient_dot_modality/include/impl/cgdm_color_gradient.hpp` |
| `test_cgdm.cpp` | correctness target | 验证边框、阈值、tie-break、public output 和 path-hit | Makefile `run_test_compare` | QEMU output | correctness gate | `test-rvv/recognition/color_gradient_dot_modality/src/test_cgdm.cpp` |
| `bench_cgdm.cpp` | bench wrapper | 生成可解析 bench 输出 | Makefile / board targets | summary / manifest | diagnostic performance input | `test-rvv/recognition/color_gradient_dot_modality/src/bench_cgdm.cpp` |

## 正确性与高效性证据链

| evidence | result | boundary |
| --- | --- | --- |
| RED | `run_test_rvv` 先失败于 RVV candidate path-hit，随后修正 oracle 与 production 语义 | test helper |
| correctness | `make -C test-rvv/recognition/color_gradient_dot_modality run_test_compare` passed | QEMU correctness + production public entry output |
| QEMU smoke | `run_bench_rvv BENCH_ARGS="--case-filter process_input_320x240 --iterations 1 --warmup-iterations 1"` passed | 日志形状，不作性能结论 |
| asm | `check_cgdm_rvv_asm` passed | 证明 production RVV helper 命中 RVV 指令 |
| diagnostic board | `cgdm_phase000_dominant_map_repeated`: helper median `3.080x` / `3.990x` | production-shaped diagnostic |
| production board | `cgdm_phase010_production_direct_repeated`: public entry median `2.600x` / `3.510x` | production direct |
| Evidence Doctor | production direct Errors=0，Warnings=0，Suggestions=4 | 可采纳 |

## closeout

正式 production 文档：`doc-rvv/recognition/color_gradient_dot_modality-RVV.zh.md`。当前 topic 收口；后续只在
profile 证明 `computeInvariantQuantizedMap()` 仍是 template creation 热点时，才恢复 `cgdm-invariant-map-rvv`。
