# Phase 000 计划：当前状态与 edge interpolation diagnostic

## 阶段意图和边界

本阶段开启 `surface/include/pcl/surface/impl/marching_cubes.hpp` 的 RVV topic。目标不是修改 production（生产源码），而是在 `test-rvv/surface/marching_cubes` 内建立可恢复的函数级评估、测试支撑、QEMU correctness（QEMU 正确性）和板卡 benchmark（性能测试）入口。

本阶段只验证 `createSurface` 风格的 cell-to-triangles 热点：8 个 leaf value 形成 cube index，edge table 命中后计算最多 12 条 edge 的插值点，随后按 tri table 追加三角点。`voxelizeData()` 的 Hoppe kd-tree 搜索和 RBF solver（求解器）不在本阶段计时边界内。

| scope item | 本阶段范围 |
| --- | --- |
| evidence role | `production-shaped diagnostic`：测试专用 helper 复刻 production helper 语义。 |
| A/B boundary | `test helper`：Std build 调 `emitSurfaceStd`，RVV build 调 `emitSurfaceCandidate`。 |
| 当前决策问题 | `RVV-vs-scalar`，先判断 edge interpolation batch 是否值得进入后续 production probe。 |
| point type / Scalar | `pcl::PointNormal`，`float` grid / vertex 坐标。 |
| layout | dense contiguous grid，`res_x * res_y * res_z` 线性数组；不验证泛型点类型 traits。 |
| production boundary | 不改 `marching_cubes.hpp`，不声明 production dispatch。 |
| 不覆盖范围 | Hoppe / RBF voxelization 成本、真实 `performReconstruction()` end-to-end、`Scalar=double`、production fallback。 |

## 当前状态清单

| item | 当前状态 | evidence |
| --- | --- | --- |
| production source | 标量 `createSurface` 每个 cell 分配两个 small vector，再逐 edge 调 `interpolateEdge`。 | `surface/include/pcl/surface/impl/marching_cubes.hpp` |
| upstream test | PCL 已有 `MarchingCubesHoppe` / `MarchingCubesRBF` smoke，但依赖 PCD 输入和 voxelize 子类。 | `test/surface/test_marching_cubes.cpp` |
| topic assets | 新 topic 尚无 Makefile、test、bench、phase docs、registry。 | `test-rvv/surface/marching_cubes/` |
| sibling quality bar | `organized_fast_mesh` 已有 `src/`、`include/`、topic-local doc suite 和 board harness。 | `test-rvv/surface/organized_fast_mesh/` |
| dirty isolation | 工作树有其它 topic 未提交改动，本阶段只触碰 marching_cubes topic 目录。 | `git status --short` |

## 假设与候选族

| candidate family | idea source | applies to | expected benefit | risk / unknown | required evidence | status | next phase |
| --- | --- | --- | --- | --- | --- | --- | --- |
| edge-interpolation-rvv | 当前源码 `createSurface` 的 12 条 edge 插值 | active cell 的 vertex interpolation | 以 RVV 批量计算 edge endpoint、value delta 和 xyz 线性插值，减少重复标量算术 | 12 条 edge 规模小，tri table 输出仍标量，收益可能被 staging 抵消 | Std/RVV correctness、asm、board repeated、Evidence Doctor | planned | Phase 000 |
| cube-index-prepass | voxel scan 中 8 个 grid value 与 iso_level 比较 | dense grid scan | 批量计算 active cell mask，减少无效 cell 进入 surface emission | 相邻 cell 取 8 值有重叠，gather/staging 可能大于收益 | 需要独立 scan bench / correctness | deferred | 若 edge-interpolation 不足或 active ratio 很低 |
| full-public-probe | `performReconstruction()` 真实 Hoppe/RBF 入口 | production public entry | 判断 helper 收益是否能穿透 voxelize 和 output construction | kd-tree / RBF solver 可能主导，且 production patch 需要用户确认 | production direct plan、fallback、board | deferred | diagnostic positive 后才进入 PI1 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| edge-interpolation-rvv | dense-grid-cell | `PointNormal` / `float` / contiguous grid | `emitSurfaceCandidate` test helper | `run_test_compare` | `run_bench_compare` only on board; QEMU bench compare disabled | planned `board_smoke` then repeated if signal close | `dump_bench_rvv` | planned summary-md or manifest | planned | create harness and run tests |
| cube-index-prepass | dense-grid-cell | grid floats | scan-only diagnostic | planned | planned | missing | missing | missing | deferred | resume if Phase 000 negative/neutral |
| full-public-probe | Hoppe/RBF public entry | `PointNormal` / production object state | `performReconstruction()` | planned | planned | missing | missing | missing | deferred | needs user-approved production integration |

## 实现和测试动作

| action | artifact / command | completion criteria |
| --- | --- | --- |
| scaffold | `Makefile`、`board.mk`、`include/marching_cubes.h`、`include/impl/marching_cubes_*`、`src/test_marching_cubes.cpp`、`src/bench_marching_cubes.cpp` | topic builds in Std and RVV modes. |
| correctness | `make -C test-rvv/surface/marching_cubes run_test_compare` | Std/RVV gtest 全部通过，candidate 输出点、三角数和 checksum 匹配 scalar reference。 |
| QEMU smoke | build bench and optionally run a small non-compare smoke only | 不把 QEMU timing 写成性能结论。 |
| asm | `make -C test-rvv/surface/marching_cubes dump_bench_rvv` | RVV bench asm 中可见 RVV 指令，归属到 test helper / bench 二进制。 |
| board | `make -C test-rvv/surface/marching_cubes board_smoke`；若方向接近阈值，再做有界 repeated | 板卡可用时完成目标硬件性能证据。 |
| doctor | `python3 test-rvv/script/evidence_doctor.py --summary-md ...` 或 topic manifest wrapper | Errors / Warnings / Suggestions 写入 result；metadata 不完整时降级说明。 |
| docs | evaluation、README、phase result、matrix、roadmap | 能从文档恢复源码、target、证据和下一 phase。 |

## Evidence Doctor 和 registry 规则

首轮可先用 `--summary-md` 对 board compare summary 做轻量 Evidence Doctor；若后续进入 production probe 或 repeated board，补 topic-local manifest wrapper。本阶段若尚无 registry，Handoff 写 `evidence_registry_status=not_available`，并列出人工检查路径。

## 板卡复跑预算和决策桶

板卡已由用户确认可用。默认先执行一次 `board_smoke`。若 summary 显示 median 在 `0.97x..1.10x` 或 Evidence Doctor warning 影响判断，最多追加一次同边界 repeated run。决策桶：

| bucket | rule |
| --- | --- |
| positive | stable `speedup >= 1.10x`，且无未解释 Error。 |
| weak_positive | `1.03x <= speedup < 1.10x`，需要后续 production-shaped 或 public probe。 |
| neutral | `0.97x <= speedup < 1.03x`。 |
| negative | `speedup < 0.97x`。 |
| unstable | bounded rerun 后跨 bucket 且无法解释。 |

## diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | `production-shaped diagnostic`。 |
| A/B boundary | `test helper`，不是 public overload。 |
| 当前决策问题 | `RVV-vs-scalar`，判断 edge interpolation helper 是否值得继续。 |
| diagnostic 是否可外推到 production | `unknown`。它复刻 `createSurface` 主体，但没有覆盖真实 `voxelizeData()` 和 public object state。 |
| comparison-boundary / baseline mismatch 风险 | 有。Std/RVV build 通过相同 test helper 与相同 synthetic grid；但不是 production dispatch。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 只有 helper 结果至少 weak_positive、正确性和 asm 均闭合时才建议进入 PI1；neutral / negative 默认先做 cube-index-prepass 或停止该 family。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 当前没有 adopted RVV family；仍需 production direct Std/RVV positive 和用户确认，不能由本阶段 clean adopt。 |

## 阶段完成条件

本阶段完成需要：test scaffold 可编译、Std/RVV correctness 通过、asm dump 可归属、板卡性能或真实阻塞已记录、Evidence Doctor 已解释、phase result / roadmap / matrix / evaluation 已更新。若 helper positive，默认下一 phase 是 PI1 前的 production boundary audit；若 helper neutral / negative，默认下一 phase 是 cube-index-prepass 或 no-production closeout 判断。

## 继续 / 停止条件

默认继续到板卡和 Evidence Doctor。只有编译失败且同轮无法修复、板卡不可达、Evidence Doctor Error 暴露证据合同不可修、dirty isolation 不安全，或继续需要修改 production/public API 并等待用户确认时才停止。
