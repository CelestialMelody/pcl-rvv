# Phase 070 result: color RGB/LUT indexed diagnostic

## 阶段结论

本阶段完成 test-only color RGB/LUT indexed diagnostic（测试专用 indexed RGB / 查找表颜色诊断），没有修改
`features/include/pcl/features/impl/shot.hpp`。新增 helper 从 indexed `PointXYZRGBA` surface 读取 RGB，按 PCL
`RGB2sRGB_LUT` / `XYZ2LAB_LUT` 公式转换为归一化 LAB，再复用 Phase 060 的 RVV LAB distance helper。

EvidenceDecision（证据决策）：`attempted / neutral-weak`。首轮 targeted board run 为 1.02x，checksum 一致；
Evidence Doctor（证据体检）Errors=0、Warnings=1、Suggestions=1。该结果说明 RGB/LUT scalar staging（标量暂存）
几乎吞掉 Phase 060 arithmetic-only（仅算术组件）的收益，不建议把这个 staging shape（暂存形态）推进到 production probe（生产探针）。

## 计划动作回填

| action | status | command / evidence | result |
| --- | --- | --- | --- |
| F1 red test | done | `make -C test-rvv/features/shot run_test_compare` | 缺少 `computeColorBinDistanceIndexedRGBScalar/RVV` 时 Std 编译失败，红灯符合预期。 |
| F2 helper implementation | done | `include/impl/shot_color.hpp` | 新增 indexed RGB/LUT scalar reference 和 RVV candidate；RVV path 的 LAB staging 成本保留在 helper 内。 |
| F3 component bench | done | `src/bench_shot.cpp` | 新增 `color_rgb_lut_indexed_component` case。 |
| F4 manifest metadata | done | `script/generate_shot_evidence_manifest.py` | 新增 case metadata，标记为 indexed AoS color point batch with scalar LAB staging。 |
| F5 asm | done | `make -C test-rvv/features/shot dump_bench_rvv` | asm 可见 LAB distance RVV 算术和 `vse64`；LUT conversion 按计划保持 scalar staging。 |
| F6 board evidence | done | targeted `run_board_bench_compare fetch_board_logs` | Std 5.3370 ms，RVV 5.2281 ms，1.02x；按计划为 neutral / weak，不追加 positive rerun。 |
| F7 docs | done | 本 result、phase README、matrix、roadmap、evaluation、README 和队列表 | 当前结论、证据边界和后续路线已同步。 |

## Correctness（正确性）

`run_test_compare` 通过 Std / RVV 两侧各 11 个 gtest。新增测试
`ShotColorRgbLutIndexedComponent.RvvMatchesScalarReferenceForIndexedColorCloud` 覆盖：

- indexed `PointXYZRGBA` 访问；
- 重复 index 的输出一致性；
- reference index 与邻域 index 相同时输出 0；
- PCL LUT 公式、归一化 LAB 和 color bin distance 的 same-chain（同构链路）。

该测试只证明 test helper 的同构正确性，不证明 production color loop 已经接入 RVV。

## 反汇编归属

`dump_bench_rvv` 通过。asm 中可见 070 复用的 LAB distance RVV 指令形态，包括 `vfsub`、`vfabs`、`vfadd`、
`vfmax`、`vfmin`、`vfwcvt` 和 `vse64`。本阶段不声明 RGB2CIELAB LUT gather 已经 RVV 化；该部分按计划仍是 scalar staging。

## Board evidence（板卡证据）

| run label | Std avg | RVV avg | speedup | doctor |
| --- | ---: | ---: | ---: | --- |
| `phase070_color_rgb_lut_indexed_smoke` | 5.3370 ms | 5.2281 ms | 1.02x | Errors=0, Warnings=1, Suggestions=1 |

Warning 是 `low_run_count`。Suggestion 是 `near_threshold_ba`，因为 1.02x 太接近 1.0，容易被测量波动反转。处理动作是按 phase plan 将该 candidate 标记为 neutral / weak，不扩大 rerun，不写 production-ready。

## Diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | component diagnostic（组件诊断）。 |
| A/B boundary | test helper / indexed RGB-LUT scalar staging + RVV LAB distance bench。 |
| 当前决策问题 | RVV-vs-scalar component A/B 和 implementation-shape。 |
| diagnostic 是否可外推到 production | 不能直接外推。它不覆盖 protected estimator state、`std::vector::push_back` 和完整 color interpolation。 |
| comparison-boundary / baseline mismatch 风险 | 存在。RVV candidate 额外写出 LAB staging arrays；production 标量路径边转换边 push。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 不允许。当前弱正向不足以支撑生产探针。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 需要。当前证据不足以 clean-adopt。 |

## 矩阵更新

`indexed RGB/LUT scalar staging + RVV LAB distance` 从 `planned` 更新为 `attempted / neutral-weak`：

- validated scope（已验证范围）：`PointXYZRGBA` indexed AoS 输入、PCL RGB2CIELAB LUT 公式、归一化 LAB、double color-bin output。
- unvalidated scope（未验证范围）：direct byte/LUT gather、production vector push、完整 color interpolation、production direct。
- rejected shape（拒绝形态）：把 LAB staging arrays 作为 color production probe 输入。该形态只有 1.02x，不能支撑接入生产。

## 继续 / 停止判断

`stop_condition_hit`：本 candidate 不建议进入 production probe；继续做 PI2 会扩大到未授权 production 且证据不支持。

`next_phase_default`：若仍不授权 production，继续更窄的 `080-interpolation-bin-selection-scalar-tail-diagnostic`，避免 Phase 050 已证伪的多组 geometry staging arrays，只检查 bin selection / scalar-tail 是否有可保留的局部收益。
