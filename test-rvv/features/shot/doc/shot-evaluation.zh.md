# SHOT 函数级评估

## S2 函数级评估

目标源码是 `features/include/pcl/features/impl/shot.hpp`，公开入口来自 `SHOTEstimation` 和 `SHOTColorEstimation` 的 `computeFeature`。调用链是 `Feature::compute` 准备输出云后进入 `computeFeature`，每个 query index（查询点索引）先检查 input point（输入点）和 local reference frame（局部参考系），再通过 `searchForNeighbors` 获取邻域，随后调用 `computePointSHOT` 生成 descriptor（描述子），最后复制 descriptor 和 reference frame 到输出点。

当前源码中的主成本分为五段：

| 阶段 | 源码位置 | RVV 价值判断 |
| --- | --- | --- |
| LRF 初始化 | `initCompute` 中的 `SHOTLocalReferenceFrameEstimation` | 可显著影响公开入口总耗时，但属于 `shot_lrf.hpp` 伴随主题；本阶段用外部 frames 收窄 descriptor 证据。 |
| 邻域搜索 | `searchForNeighbors` | 搜索树主导，当前 topic 不接管，只在 production-shaped bench 中保留真实入口成本。 |
| shape bin 距离 | `createBinDistanceShape` | 每个邻域点对 normal 和 z-axis 做 dot（点积）并映射到 shape bin，适合做有限 mask（掩码）和批量 dot 诊断。 |
| descriptor 插值 | `interpolateSingleChannel` / `interpolateDoubleChannel` | 每个邻域点计算半径、x/y/z 投影、inclination / azimuth（倾角 / 方位角）和多邻接 bin 写入；存在 scatter-like histogram（类似离散写入的直方图）风险，是首要诊断对象。 |
| 归一化与输出复制 | `normalizeHistogram` 与 `computeFeature` 的 descriptor / `rf` copy | 352 / 1344 维顺序循环，适合后续作为低风险 component ablation（组件消融）候选，但必须先证明它不是被搜索和插值稀释。 |

当前判断：本 topic 已完成 shape-bin indexed gather 的 PI2-PI5 production integration loop（生产接入闭环），并在 PI3 按用户确认回滚 production patch（生产补丁）。Phase 000 建立了固定 LRF 公开入口 scaffold；Phase 010 证明 descriptor normalization component（描述子归一化组件）在板卡上有稳定正向，352 维约 1.58x-1.63x，1344 维约 1.49x-1.52x。Phase 020 证明 shape-bin SoA component（按字段拆开的 shape-bin 组件）稳定 2.38x-2.47x。Phase 030 证明 shape-bin contiguous AoS component（连续结构数组组件）稳定 1.76x-1.90x。Phase 040 证明 shape-bin indexed gather component（按索引离散加载组件）稳定 1.65x-1.86x。Phase 050 证明 interpolation geometry staging arrays（插值几何暂存数组）修正后仍只有 0.97x，不建议作为生产探针输入。Phase 060 证明 color LAB distance arithmetic（颜色 LAB 距离算术）组件稳定 1.10x-1.23x，但 Phase 070 加入 indexed RGB/LUT scalar staging（按索引读取 RGB / 查找表标量暂存）后只有 1.02x，不建议该 staging 形态作为生产探针输入。Phase 080 的 interpolation bin-selection scalar-tail staging（插值桶选择标量尾段暂存）三次定向板卡 run 为 0.84x、1.12x、1.17x，decision bucket（决策桶）不稳定，不建议作为生产探针输入。PI2 接入 `createBinDistanceShape` 后，production-detail（生产细节 helper）为 1.07x weak-positive（弱正向），但 production-public（生产公开入口）`public_shot352_fixed_lrf` / `public_shot1344_fixed_lrf` 为 0.98x / 0.99x，两个 public case 的 Evidence Doctor（证据体检）均给出退化 Error。当前结论是 `rollback/no-production`：`shot.hpp` 保持标量 production path，不创建正式 `doc-rvv/features/shot-RVV.zh.md`。

## Traceability Map（可追踪性地图）

| 符号 / 文件 | 层级 | 作用 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- |
| `SHOTEstimation::computeFeature` | production public entry | SHOT352 公开入口，执行邻域搜索、descriptor 计算和输出复制。 | production boundary（生产边界） | `features/include/pcl/features/impl/shot.hpp` |
| `SHOTColorEstimation::computeFeature` | production public entry | SHOT1344 公开入口，包含 shape + color descriptor。 | production boundary（生产边界） | `features/include/pcl/features/impl/shot.hpp` |
| `createBinDistanceShape` | production helper | normal dot 和 shape bin 距离计算。 | candidate target（候选目标） | `features/include/pcl/features/impl/shot.hpp` |
| `interpolateSingleChannel` / `interpolateDoubleChannel` | production helper | 邻域点的 quadrilinear interpolation（四线性插值）和 histogram 写入。 | candidate target / numerical risk（数值风险） | `features/include/pcl/features/impl/shot.hpp` |
| `normalizeHistogram` | production helper | descriptor L2 归一化。 | component ablation target（组件消融目标） | `features/include/pcl/features/impl/shot.hpp` |
| `include/shot.h` | RVV test asset | 聚合测试支撑入口。 | test support boundary（测试支撑边界） | `test-rvv/features/shot/include/shot.h` |
| `src/test_shot.cpp` | RVV test asset | 固定 LRF correctness、indices 子集、NaN fallback 和 production-detail direct tests。 | QEMU / board correctness（正确性） | `test-rvv/features/shot/src/test_shot.cpp` |
| `src/bench_shot.cpp` | bench wrapper | 合成输入上的 SHOT352 / SHOT1344 public bench、component bench 和 `production_shape_bin_direct`。 | board performance diagnostic / production evidence（板卡性能诊断 / 生产证据） | `test-rvv/features/shot/src/bench_shot.cpp` |
| `include/impl/shot_normalize.hpp` | test-only component helper | 归一化标量参考链路和 RVV 候选。 | component diagnostic（组件诊断） | `test-rvv/features/shot/include/impl/shot_normalize.hpp` |
| `include/impl/shot_shape_bin.hpp` | test-only component helper | shape-bin SoA、连续 AoS、indexed gather 标量参考链路和 RVV 候选。 | component diagnostic（组件诊断） | `test-rvv/features/shot/include/impl/shot_shape_bin.hpp` |
| `include/impl/shot_interpolate.hpp` | test-only component helper | interpolation geometry staging 和 bin-selection scalar-tail staging 标量参考链路和 RVV 候选。 | component diagnostic（组件诊断） | `test-rvv/features/shot/include/impl/shot_interpolate.hpp` |
| `include/impl/shot_color.hpp` | test-only component helper | normalized LAB color-bin distance 标量参考链路和 RVV 候选。 | component diagnostic（组件诊断） | `test-rvv/features/shot/include/impl/shot_color.hpp` |
| `script/generate_shot_evidence_manifest.py` | evidence wrapper | 把 SHOT board smoke 日志转成 Evidence Doctor manifest。 | evidence summary（证据摘要） | `test-rvv/features/shot/script/generate_shot_evidence_manifest.py` |

## 测试计划和 bench 计划

| 测试 / target | 层级 | 作用 |
| --- | --- | --- |
| `run_test_compare` | production-shaped diagnostic + production-detail correctness | 分别构建 Std / RVV，验证固定 LRF 下 SHOT352 / SHOT1344 descriptor、component same-chain 和 PI2 production-detail tests。 |
| `run_test_shape_bin` | shape-bin correctness alias | 覆盖 SoA / AoS / indexed diagnostic 和 `ShotShapeBinProductionDetail.*`。 |
| `dump_bench_rvv` | asm attribution（反汇编归因）入口 | 构建 RVV bench 并导出 RVV 指令列表；PI2 已在 production `createBinDistanceShape` 符号附近确认 `vluxei32.v` / `vcpop.m`。 |
| `board_smoke` | board correctness + board bench smoke | 板卡执行测试和一次 Std/RVV bench compare，作为后续 repeated summary 和 Evidence Doctor 的输入。 |
| `run_evidence_doctor` | evidence validation（证据校验） | 生成 `log/board/evidence_manifest.json` 并输出 `log/board/evidence_doctor.md`；Error / Warning 需要在 phase result 中解释或降级。 |

## 当前诊断证据链

| 证据 | 当前结果 | 结论边界 |
| --- | --- | --- |
| QEMU correctness | PI2 后 `run_test_shape_bin` 通过 Std / RVV 两侧各 6 个 gtest，`run_test_compare` 通过 Std / RVV 两侧各 15 个 gtest。 | 证明固定 LRF 公开入口、所有既有 component diagnostic 和 production-detail fallback / direct tests 可复现，不证明目标硬件性能。 |
| 反汇编 | PI2 历史 asm 曾在 production `createBinDistanceShape` 符号附近看到 `vluxei32.v`、`vfwcvt.f.f.v` 和 `vcpop.m`；PI3 回滚后 `shot.hpp` 不再包含 production RVV helper。 | PI2 asm 只作为 historical production probe（历史生产探针）归因，不代表当前 production path。 |
| 板卡 smoke / rerun | Milkv-Jupiter 历史 component：normalization 稳定 1.49x-1.63x；shape-bin SoA 2.38x-2.47x、AoS 1.76x-1.90x、indexed gather 1.65x-1.86x。PI2 接入后：`production_shape_bin_direct` 为 1.07x，`public_shot352_fixed_lrf` 为 0.98x，`public_shot1344_fixed_lrf` 为 0.99x。补充 side-run：`board-shot-production-shape-bin-direct-side-20260825-once` 为 0.94x，`board-shot-public-shot352-side-20260825-once` 为 0.98x，`board-shot-public-shot1344-side-20260825-once` 为 0.99x。 | shape-bin 局部收益没有转成 production-public speedup；当前 production patch 不建议采纳。 |
| Evidence Doctor | PI2 production-detail Doctor 为 Errors=0、Warnings=1；两个 production-public Doctor 均为 Errors=1、Warnings=1，Error 是退化频率。 | public Error 已阻断采纳；PI3 已按用户确认回滚。 |

补充 side-run 证据路径如下。它们是 registry freshness（证据登记新鲜度）的一部分，但因 run count（复跑次数）为 1，只作为回滚判断的辅助 historical evidence（历史证据）：

- `test-rvv/features/shot/log/board/board-shot-production-shape-bin-direct-side-20260825-once/analyze_bench_compare.log`
- `test-rvv/features/shot/log/board/board-shot-production-shape-bin-direct-side-20260825-once/evidence_manifest.json`
- `test-rvv/features/shot/log/board/board-shot-production-shape-bin-direct-side-20260825-once/evidence_doctor.md`
- `test-rvv/features/shot/log/board/board-shot-public-shot352-side-20260825-once/analyze_bench_compare.log`
- `test-rvv/features/shot/log/board/board-shot-public-shot352-side-20260825-once/evidence_manifest.json`
- `test-rvv/features/shot/log/board/board-shot-public-shot352-side-20260825-once/evidence_doctor.md`
- `test-rvv/features/shot/log/board/board-shot-public-shot1344-side-20260825-once/analyze_bench_compare.log`
- `test-rvv/features/shot/log/board/board-shot-public-shot1344-side-20260825-once/evidence_manifest.json`
- `test-rvv/features/shot/log/board/board-shot-public-shot1344-side-20260825-once/evidence_doctor.md`

## Production probe 前置条件

`PI1-shape-bin-indexed-production-probe-plan` 已冻结窄范围生产探针，PI2 已按该范围执行，PI3 已按用户确认回滚。当前 production 不接 SHOT RVV path；PI2 证据只作为不采纳 shape-bin indexed gather 的历史依据。

## Doc suite role inventory（文档套件职责清单）

| role | 状态 | 路径 |
| --- | --- | --- |
| topic_navigation | standalone | `test-rvv/features/shot/README.zh.md` |
| testing_overview | standalone | `test-rvv/features/shot/doc/testing-overview.zh.md` |
| correctness_tests | standalone | `test-rvv/features/shot/doc/correctness-tests.zh.md` |
| benchmark_and_evidence | standalone | `test-rvv/features/shot/doc/benchmark-and-evidence.zh.md` |
| optimization_evidence | standalone | `test-rvv/features/shot/doc/optimization-evidence.zh.md` |
| optimization_roadmap | standalone | `test-rvv/features/shot/doc/optimization-roadmap.zh.md` |
| test_support_code_map | standalone | `test-rvv/features/shot/doc/test-support-code-map.zh.md` |
| phase_index | standalone | `test-rvv/features/shot/doc/phases/README.zh.md` |
| evaluation_diagnostic | standalone | `test-rvv/features/shot/doc/shot-evaluation.zh.md` |
| production_topic_doc | not_applicable with evidence | 当前没有 adopted production behavior；PI3 已回滚 production patch。 |

## Production topic doc 适用性

当前没有 adopted production behavior（已采用生产行为），PI3 已回滚 production patch（生产补丁），因此 `doc-rvv/features/shot-RVV.zh.md` 当前不适用，不创建。PI2 的生产数据主归属在 `test-rvv/features/shot/doc/phases/PI2-shape-bin-indexed-production-probe/result.zh.md`，PI3 回滚结论主归属在 `test-rvv/features/shot/doc/phases/PI3-shape-bin-production-rollback-closeout/result.zh.md` 和本 evaluation 文档。
