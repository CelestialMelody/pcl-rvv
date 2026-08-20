# bilateral_upsampling RVV 主题文档

## 当前状态

`surface/include/pcl/surface/impl/bilateral_upsampling.hpp` 里的 color-gather RVV family 已被用户确认保留，当前状态是 `adopted production behavior`。

当前采用范围是 RGB/RGBA exact family（精确 RGB/RGBA 点型族）的 `pcl::PointXYZRGB -> pcl::PointXYZRGB`、`pcl::PointXYZRGBA -> pcl::PointXYZRGBA`、`pcl::PointXYZRGB -> pcl::PointXYZRGBA` 与 `pcl::PointXYZRGBA -> pcl::PointXYZRGB` 生产公开入口，且点型满足 `RVVXYZAoSFloatLayout`。其它模板实例或其它布局仍回退到标量路径。

## 函数入口

公开入口是 `pcl::BilateralUpsampling<PointInT, PointOutT>::process`。它完成输入检查、投影矩阵求逆，然后调用 `performProcessing`。

`performProcessing` 先走 color-gather RVV helper；若点型/layout 或运行时 gate 不满足，直接落到标量 helper。旧 `bilateralUpsamplingPerformProcessingRVV` 只保留为历史兼容壳，供旧 bench 标签和外部符号引用，不再参与当前生产分流。

## 标量路径

标量实现对每个 `(x, y)` 像素扫描局部窗口，按 `computeDistances` 预计算出的 depth / RGB 查表累加 `sum += weight * z` 和 `norm_sum += weight`。若 `norm_sum != 0`，再用 `unprojection_matrix_ * [x*depth, y*depth, depth]` 写出 xyz；RGB 直接复制中心点。

`computeDistances` 里的 `std::exp` 是 per-process 预计算，不是逐像素热点。

## 当前采用的优化方式

| 维度 | 当前状态 | 采用或暂缓原因 | 证据 | 边界 / 下一步 |
| --- | --- | --- | --- | --- |
| dispatch / fallback | `adopted` | 先尝试 color-gather RVV helper，失败后直接回退标量；旧 `bilateralUpsamplingPerformProcessingRVV` 仅作兼容壳 | `surface/include/pcl/surface/impl/bilateral_upsampling.hpp` | 只覆盖 RGB/RGBA exact family 和 AoS float layout；其它模板实例走标量 |
| 颜色 staging | `adopted` | 把 RGB 差值与查表偏移搬进 RVV lane 内，减少标量 staging | `vlse8.v`、`vzext.vf2`、`vmaxu.vv`、`vminu.vv`、`vluxei16.v` | 只对 RGB/RGBA exact-family 点型成立 |
| depth / reduction | `adopted` | depth 仍用 strided load，`z` 用 strided load，finite mask 对齐 `std::isfinite`，最后用 `vfredusum.vs` 规约 | `vlse32.v`、`vfabs.v`、`vmflt.vf`、`vfredusum.vs` | 保持窗口累加和 NaN / infinity fallback 语义 |
| exact gate | `adopted` | 先把生产证据闭合在已验证 RGB/RGBA 点型族上 | `kBilateralUpsamplingRgbPoint` + `RVVXYZAoSFloatLayout` | 下一步其它点类型扩展必须新开 phase |
| old RVV helper | `compatibility shim` | 仅为旧 bench / 外部符号保留，不再承载生产 fallback | `bilateralUpsamplingPerformProcessingRVV` | 后续若清理，需要单独 phase 和再验证 |

## 覆盖范围与 fallback

| 范围 | 当前状态 | 说明 |
| --- | --- | --- |
| `PointXYZRGB -> PointXYZRGB` / `PointXYZRGBA -> PointXYZRGBA` | adopted | same-type 生产公开入口已证实正向，当前采用范围 |
| `PointXYZRGB -> PointXYZRGBA` / `PointXYZRGBA -> PointXYZRGB` | adopted | phase 073 已补 production direct correctness、板卡和 Evidence Doctor；alpha 不作为本阶段写回语义 |
| 其它 `PointInT` / `PointOutT` 模板实例 | scalar-only | 不满足 RGB gate 或 AoS float layout 时直接走标量 |
| 空输入、非法窗口、非组织点云 | scalar-only | 维持原语义，helper 直接返回 false 或走标量 |
| `Scalar=double`、其它点型、其它布局 | not covered | 不在当前 adopted 范围内，需下一 phase 扩展 |

## 数值算例

以一个窗口内的单个 lane 为例，RVV helper 会把某个 `(x_w, y_w)` 邻居点的 `r/g/b` 读进 lane，计算颜色差 `d_color`，再从 RGB 查表取出 `rgb_w`，与 depth weight 相乘得到 `w`，最后和 `z` 一起进入规约。这样每个 lane 对应一个邻居点，chunk 的最后再合并到 `sum` 和 `norm_sum`。

## 板卡结果展示

性能结论只来自板卡或目标硬件；QEMU 只作为 correctness（正确性）、日志形状和路径命中证据。当前 production direct（真实生产路径）板卡摘要来自 phase 073 刷新的 `test-rvv/surface/bilateral_upsampling/log/board/analyze_bench_compare.log`，speedup 按同一 case 内 `Std avg / RVV avg` 计算。

| production public case | Std avg | RVV avg | speedup | 结论 |
| --- | ---: | ---: | ---: | --- |
| `PointXYZRGB 80x60 w3 dense` | 6.1123 ms | 4.8681 ms | 1.26x | positive |
| `PointXYZRGB 120x90 w4 holes` | 25.7178 ms | 21.2556 ms | 1.21x | positive |
| `PointXYZRGBA 180x120 w5 dense` | 75.3833 ms | 63.6616 ms | 1.18x | positive |
| `PointXYZRGB -> PointXYZRGBA 120x90 w4 holes` | 26.3132 ms | 21.3591 ms | 1.23x | positive |
| `PointXYZRGBA -> PointXYZRGB 120x90 w4 dense` | 26.0464 ms | 21.3000 ms | 1.22x | positive |

| steady public case | Std avg | RVV avg | speedup | 结论 |
| --- | ---: | ---: | ---: | --- |
| `PointXYZRGB 80x60 w3 dense` | 6.6024 ms | 5.0495 ms | 1.31x | positive |
| `PointXYZRGB 120x90 w4 holes` | 26.0561 ms | 21.5517 ms | 1.21x | positive |
| `PointXYZRGBA 180x120 w5 dense` | 77.4845 ms | 62.1629 ms | 1.25x | positive |
| `PointXYZRGB -> PointXYZRGBA 120x90 w4 holes` | 26.0141 ms | 21.5678 ms | 1.21x | positive |
| `PointXYZRGBA -> PointXYZRGB 120x90 w4 dense` | 26.1494 ms | 21.5923 ms | 1.21x | positive |

Phase 060 的 bench-local precursor（局部 helper 前置证据）为 `1.31x / 1.28x / 1.07x`，说明 color-gather 族在接入 production 前已经比旧 helper-only / nan-mask 路线更有希望。最终采纳不依赖这组 precursor，而依赖 phase 070 的 public / steady production direct 结果。

Phase 070 是采纳时的历史 production direct 证据，public 为 `1.38x / 1.22x / 1.02x`，steady public 为 `1.34x / 1.24x / 1.06x`，Evidence Doctor 为 `Errors=0`、`Warnings=0`、`Suggestions=1`。Phase 072 在 finite mask 修复和 same-type gate alignment 后刷新板卡。Phase 073 扩展交叉 RGB/RGBA 后，当前 public 为 `1.26x / 1.21x / 1.18x / 1.23x / 1.22x`，steady public 为 `1.31x / 1.21x / 1.25x / 1.21x / 1.21x`，Evidence Doctor 为 `Errors=0`、`Warnings=0`、`Suggestions=0`。

Phase 071 在接入后补齐了 finite mask correctness：color-gather helper 现在与标量 `std::isfinite` 一样跳过 NaN 和 `+/-infinity`。Phase 072 把 production RVV gate 收窄到 same-type RGB/RGBA，phase 073 又用交叉 production direct 证据把 gate 扩展到 RGB/RGBA exact family。当前二进制已完成 QEMU correctness 13/13、asm attribution、QEMU bench log-shape smoke 和板卡 `board_smoke` freshness。

## 正确性与高效性证据链

| 项 | 证据 |
| --- | --- |
| 正确性 | `test-rvv/surface/bilateral_upsampling/log/qemu/run_test_std.log`、`test-rvv/surface/bilateral_upsampling/log/qemu/run_test_rvv.log`，`run_test_compare` Std/RVV 13/13 passed |
| 反汇编 | `test-rvv/surface/bilateral_upsampling/build/asm/riscv/bench_bilateral_upsampling_rvv.asm`，可见 `vlse8.v`、`vzext.vf2`、`vmaxu.vv`、`vminu.vv`、`vluxei16.v`、`vfabs.v`、`vmflt.vf`、`vfredusum.vs` |
| 板卡性能 | phase 073 `test-rvv/surface/bilateral_upsampling/log/board/analyze_bench_compare.log`，public 为 `1.26x / 1.21x / 1.18x / 1.23x / 1.22x`，steady public 为 `1.31x / 1.21x / 1.25x / 1.21x / 1.21x` |
| Evidence Doctor | phase 073 `test-rvv/surface/bilateral_upsampling/doc/phases/073-cross-rgb-rgba-production-probe/evidence-doctor.md`，`Errors=0`、`Warnings=0`、`Suggestions=0` |
| 证据边界 | 覆盖 RGB/RGBA exact family、`Scalar=float`、AoS float layout 和 organized RGBD grid；其它模板实例仍按 fallback 矩阵处理 |
| 风险处理 | phase 070 大 RGBA public case 的 near-threshold 弱收益已由后续刷新转正；后续扩大其它点型、layout 或规模前仍需要新 phase 重跑 production direct、asm、board 和 Evidence Doctor |

## Traceability Map

| 符号 / 文件 | 层级 | 作用 | 调用者 / 上游入口 | 被调用者 / 下游消费者 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- | --- | --- |
| `pcl::BilateralUpsampling<PointInT, PointOutT>::process` | production public entry | 公开入口和最终调度点 | 上游调用方 | `performProcessing` | production boundary | `surface/include/pcl/surface/impl/bilateral_upsampling.hpp` |
| `pcl::BilateralUpsampling<PointInT, PointOutT>::performProcessing` | production dispatch / fallback | 选择 color-gather RVV、历史 RVV 或标量 | `process` | RVV helper / Std helper | fallback coverage | 同上 |
| `bilateralUpsamplingPerformProcessingColorGatherRVV` | production RVV helper | 当前 adopted RVV 组织方式 | `performProcessing` | `vfredusum` 规约、输出写回 | asm attribution / production direct | 同上 |
| `bilateralUpsamplingPerformProcessingStd` | production Std helper | 回退标量实现 | `performProcessing` | 标量输出 | fallback coverage | 同上 |
| `test-rvv/surface/bilateral_upsampling/src/test_bilateral_upsampling.cpp` | correctness test | QEMU 对拍与边界验证 | Make target | 证实 fallback / 正确性 | correctness gate | `test-rvv/surface/bilateral_upsampling/src` |
| `test-rvv/surface/bilateral_upsampling/src/bench_bilateral_upsampling.cpp` | bench wrapper | public / steady / helper 证据输入 | Make target / board target | 生成 board smoke | board performance / asm attribution | 同上 |
| `test-rvv/surface/bilateral_upsampling/log/board/analyze_bench_compare.log` | evidence output summary | 板卡性能摘要 | board smoke | 生产接入判断 | production direct performance | `log/board` |
| `test-rvv/surface/bilateral_upsampling/doc/bilateral_upsampling-evaluation.zh.md` | evaluation | 候选取舍和边界审计 | 文档读者 | closeout 与恢复入口 | decision audit | `doc/` |
| `test-rvv/surface/bilateral_upsampling/doc/phases/070-production-detail-color-gather-production-probe/result.zh.md` | phase result | PI5 结论和用户确认记录 | phase loop | 当前 adopted 语义来源 | production decision | `doc/phases/...` |
| `test-rvv/surface/bilateral_upsampling/doc/phases/071-production-finite-mask-correctness-refresh/result.zh.md` | phase result | finite mask correctness refresh | phase loop | finite mask correctness 来源 | production correctness；board freshness 已由 phase 072 覆盖 | `doc/phases/...` |
| `test-rvv/surface/bilateral_upsampling/doc/phases/072-production-same-type-gate-alignment/result.zh.md` | phase result | same-type gate alignment | phase loop | phase 073 之前的板卡 freshness | historical production direct performance / doctor | `doc/phases/...` |
| `test-rvv/surface/bilateral_upsampling/doc/phases/073-cross-rgb-rgba-production-probe/result.zh.md` | phase result | cross RGB/RGBA 扩展和当前板卡 freshness | phase loop | 当前二进制 performance freshness | production direct performance / doctor | `doc/phases/...` |

## 生产 closeout

| 项 | 最终状态 | 证据 |
| --- | --- | --- |
| 生产补丁范围 | adopted | `surface/include/pcl/surface/impl/bilateral_upsampling.hpp`，新增 color-gather RVV helper 和 dispatch 顺序；旧 RVV 名称仅保留兼容壳 |
| 覆盖范围 | adopted | RGB/RGBA exact family，AoS float layout，`process` 真实公开入口 |
| 不覆盖范围 | retained scalar | 其它点型、其它布局、`Scalar=double`、未满足 gate 的模板实例 |
| fallback 矩阵 | adopted | color-gather 失败后直接回到标量 helper；旧 RVV 名称只用于兼容调用 |
| production direct 证据 | adopted / refreshed | phase 073 `run_test_compare` 13/13、`dump_bench_rvv`、`board_smoke`、Evidence Doctor `Errors=0`、`Warnings=0`、`Suggestions=0` |
| 结论差异 | no contradiction | 诊断阶段的旧 family 负向结果已经被 color-gather 正向结果取代 |
| 回退策略 | retained | 如后续需要撤回 adopted patch，可直接回滚该 production 分支并保留 topic-local 证据 |

## 后续方向

当前 adopted 结论只覆盖 RGB/RGBA exact family 和当前 layout。若要继续扩大到更多点类型、更多 `Scalar` 或其它 layout，需要新开 point-type / layout expansion phase，并重新补 production direct、板卡、asm 和 Evidence Doctor 证据。
