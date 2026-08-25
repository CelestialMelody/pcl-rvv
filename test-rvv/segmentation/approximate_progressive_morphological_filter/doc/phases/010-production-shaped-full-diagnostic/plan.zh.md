# 010 production-shaped-full-diagnostic 计划

## 阶段意图和边界

本阶段建立 production-shaped diagnostic（生产形态诊断，尽量复用真实入口状态和调用方式的测试专用诊断）。目标是在 `test-rvv/segmentation/approximate_progressive_morphological_filter/` 内组合 000 阶段的 z-min、window-open 和 tail-compress helper，模拟 `ApproximateProgressiveMorphologicalFilter<PointT>::extract` 的关键数据流：输入点云构造初始 `A`，按多个 `half_size / height_threshold` 连续执行 morphological open（形态学开运算），每轮按 `filtered` grid 更新 `ground`，随后 `A.swap(Zf)`。

本阶段仍不修改 `segmentation/include/pcl/segmentation/impl/approximate_progressive_morphological_filter.hpp`。它不证明 public entry（公开入口）真实命中 RVV，不覆盖泛型 `PointT`、`Scalar=double`、indices subset、OpenMP 调度或 production fallback（生产回退路径）。

## 当前状态清单

| 项目 | 当前事实 | 路径 |
| --- | --- | --- |
| component correctness | Std / RVV QEMU 均通过 4 个 component tests | `log/qemu/run_test_std.log`、`log/qemu/run_test_rvv.log` |
| component board | z-min dense 2.27x、z-min non-dense 2.45x、tail-compress 2.32x，window-open 1.02x | `log/board/repeated/summary.md` |
| Evidence Doctor | Errors=0，Warnings=1，Suggestions=1；warning 指向 window-open group outlier | `log/board/repeated/evidence_doctor.md` |
| asm | RVV 诊断二进制中已有 floor conversion、compress、gather、load/store 指令 | `build/asm/riscv/bench_apmf_rvv.asm` |
| production 状态 | 未修改 production；当前只允许 topic-local 测试资产和文档 | `git status --short --untracked-files=all -- test-rvv/segmentation/approximate_progressive_morphological_filter` |

## validated_scope / unvalidated_scope

validated_scope（本阶段准备证明）：

- row source：full input cloud（完整输入点云）初始化 grid，当前 `ground` vector 逐轮过滤。
- point type / Scalar / layout：`pcl::PointXYZ` / float / AoS（结构数组）输入，Eigen `MatrixXf` column-major grid。
- entry shape（入口形态）：test-only full pipeline helper，不是 public overload。
- scale：默认板卡规模 `262144` 点、`160x160` grid、3 个 half-size 阶段，必要时用小规模 QEMU smoke。

unvalidated_scope（本阶段不证明）：

- production public dispatch、`initCompute` 和真实 PCL 参数对象。
- 泛型 `PointT`、PointXYZ-like traits gate、`Scalar=double`。
- `indices_` 子集、organized cloud、多线程 OpenMP 与 RVV 混合收益。
- production 中 `A(j,k) != quiet_NaN()` / `Z(j,k) != quiet_NaN()` 的 NaN 比较语义是否需要独立修正。

point_type_expansion_queue：若本阶段 positive，只能进入 PI1 或后续 point-type / fallback plan；泛型生产门控前必须读取泛型点类型策略并补 traits、offset、fallback、asm、board 和 Evidence Doctor。

## 假设与候选族

| candidate family | 假设 | 风险 / 未知 |
| --- | --- | --- |
| `full-pipeline-rvv-components` | 把 z-min、window-open、tail-compress 串联后，z-min 和 tail-compress 的收益仍能抵消 window-open 中性成本 | 多轮 window-open 可能成为主耗时，component speedup 不能外推 |
| `hybrid-full-diagnostic` | 若 full RVV 组合收益弱，可保留 z-min + tail-compress RVV，window-open 走 Std，测试其是否更稳 | 两条路径要保持同一 `A.swap(Zf)` 和 `ground` 更新顺序，bench label 必须避免混淆 |

## 本阶段优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `full-pipeline-rvv-components` | full input cloud + current ground vector | `PointXYZ` / float / AoS + Eigen column-major | test-only production-shaped full helper | 新增 full-pipeline same-chain test，覆盖 dense 与 non-dense 输入 | 新增 full-pipeline bench case | 5-run repeated board summary，positive / neutral / negative 分桶 | `dump_bench_rvv` 检查关键 RVV 指令仍存在 | 生成 full diagnostic manifest 并跑 Evidence Doctor | planned |
| `hybrid-full-diagnostic` | 同上 | 同上 | test-only hybrid helper | 若 full RVV 中性或退化则新增 same-chain test | 若 full RVV 中性或退化则新增 hybrid bench case | 视 full 结果决定是否同轮复跑 | 同上 | 同上 | deferred |

## 实现和测试动作

| action | 产物 | 完成判据 |
| --- | --- | --- |
| 写 RED 测试 | `src/test_apmf.cpp` | 新增 full-pipeline 测试先失败，失败原因是缺少 full helper 或 helper 未实现 |
| 实现 Std full helper | `include/impl/apmf_components.hpp` | hand-derived fixture 下能产出稳定 ground indices 和 grid checksum |
| 实现 RVV full helper | `include/impl/apmf_components.hpp` | RVV helper 复用 000 component helper，QEMU same-chain 与 Std 对拍通过 |
| 扩展 bench | `src/bench_apmf.cpp` | 新增 full-pipeline label、checksum 和 CLI 参数，QEMU smoke 只用于日志形状 |
| 扩展 manifest wrapper | `script/generate_apmf_board_evidence_manifest.py` | summary / manifest 能解析新增 case 并标出 evidence role、A/B boundary、timer boundary |
| 板卡复跑 | `log/board/full-pipeline-repeated/` 或等价 run label | 5-run repeated summary + Evidence Doctor 生成；性能结论只来自板卡 |
| 更新 result / roadmap / matrix | phase docs | result 回填 correctness、asm、board、doctor 和继续 / 停止决策 |

## Evidence Doctor 和 registry 规则

本阶段新增 full-pipeline case 后，必须更新 topic-local manifest wrapper，使新增 case 的 `case_kind`、`evidence_role`、`timer_boundary` 和 `decision_bucket` 可被共享 Evidence Doctor 读取。若仍复用 component repeated 目录，必须在 summary 和 manifest 中明确 run label；若使用新目录，Makefile 变量应允许覆盖输出路径。

Evidence Doctor 出现 Error 时先修复或降级证据，不进入 production；Warning 必须在 result 中解释。window-open neutral 是已知风险，不能被其它组件收益掩盖。

## 板卡复跑预算和决策桶

本阶段板卡预算为 5 run，默认参数：

```text
BENCH_ARGS='--size 262144 --grid-rows 160 --grid-cols 160 --half 4 --iterations 8 --warmup 2'
```

decision bucket（决策桶）：

- `positive`：full-pipeline median speedup >= 1.20x，且 min >= 1.05x、checksum 一致、Evidence Doctor 无 Error。
- `weak-positive`：median 1.05x 到 1.20x，且维护成本仍低；只能支持有界生产探针讨论。
- `neutral`：0.95x 到 1.05x；不支持直接接 production，可触发 hybrid-full-diagnostic。
- `negative`：低于 0.95x；不接 production，除非另有 bounded probe 理由。
- `unstable`：5 run 内方向反复或 Evidence Doctor warning 无法解释；降级为诊断证据。

## diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | `production-shaped diagnostic`，仍是 test-only helper |
| A/B boundary | `production-shaped helper`，不是 public overload |
| 当前决策问题 | `RVV-vs-scalar`、`implementation-shape` 和是否值得进入 PI1 |
| diagnostic 是否可外推到 production | 只能弱外推到“值得写 PI1 计划”。真实 production 还需 public dispatch、fallback、`__RVV10__`、泛型点类型和 production direct bench |
| comparison-boundary / baseline mismatch 风险 | 有。helper 会模拟关键状态，但未经过真实 `ApproximateProgressiveMorphologicalFilter` 对象生命周期和 OpenMP 调度 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 若 full 结果 neutral / negative，默认不直接进 production；若 hybrid 正向且生产改动足够小，可在 result 中提出 PI1 但不自动修改 production |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 需要。进入 PI1 后还必须补 production public 或 production detail 证据 |

## 继续 / 停止条件

默认继续执行 RED-GREEN、QEMU correctness、asm、板卡 5-run 和 Evidence Doctor。合法停止条件：新增 helper 需要修改 production / public API；板卡或工具不可达；Evidence Doctor Error 无法修复；full diagnostic 与 component 证据矛盾到需要人工判断；或者 PI1 生产接入需要用户明确确认。

next_phase_default：完成本阶段并且 full-pipeline positive 时，进入 `020-production-integration-plan`；若 full-pipeline neutral / negative，但 hybrid 值得查，进入 `020-hybrid-full-diagnostic`；若两者均不支持生产探针，进入 no-production diagnostic closeout 文档阶段。
