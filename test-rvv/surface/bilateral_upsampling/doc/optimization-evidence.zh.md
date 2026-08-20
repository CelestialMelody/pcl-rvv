# bilateral_upsampling 优化证据索引

本文承担 `optimization_evidence` role：按 candidate family 记录已经尝试、拒绝、暂缓或采纳的 RVV 方式，并把每条结论连接到源码、测试、bench、asm、板卡和 Evidence Doctor。路线图中的未来想法不在本文写成已证实结果。

## 当前结论摘要

当前 adopted family 是 `production detail color-gather`：它把 RGB 距离和 RGB 查表挪进 RVV lane，保留 depth strided load、finite-depth mask、`vfredusum` 规约和标量 unprojection。phase 073 RGB/RGBA exact-family production public / steady public 板卡结果为 `1.26x / 1.21x / 1.18x / 1.23x / 1.22x` 和 `1.31x / 1.21x / 1.25x / 1.21x / 1.21x`，Evidence Doctor 为 `Errors=0`、`Warnings=0`、`Suggestions=0`。旧 helper family 的 public / steady / helper-only / nan-mask 证据均已降为历史对照。

## 优化方式总表

| candidate family | 代码路径 | bench / test 路径 | board evidence | asm evidence | Evidence Doctor | decision | 边界 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| staged-window-reduction | `include/impl/bilateral_upsampling_core.hpp::processCandidate` | table window bench + diagnostic gtest | `0.90x / 0.91x / 0.90x` | `vle32/vfmul/vfredusum` | phase 000 `Errors=3` | rejected | staging 成本抵消收益，未接 production |
| column-stride-depth-direct | `processDirectDepthCandidate` | direct-depth bench + diagnostic gtest | `1.04x / 1.16x / 1.28x` | `vlse32/vmfeq/vmerge/vfmul/vfredusum` | phase 010 `Errors=0`、`Warnings=12`、`Suggestions=1` | historical precursor | 只证明减少 depth staging 有希望，不能替代 production direct |
| old production exact-gate helper | `surface/include/pcl/surface/impl/bilateral_upsampling.hpp::bilateralUpsamplingPerformProcessingRVV` | production public / steady / helper-only | historical public `0.93x / 0.90x / 0.97x`；phase 040 helper-only rerun 为 `0.91x / 0.91x / 0.95x` | old helper 有 `vlse32/vmfeq/vmerge/vfmul/vfredusum` | phase 020/040/050 不支持采纳 | rejected / superseded | 保留作 color-gather 失败后的历史 RVV 回退路径 |
| public shell overhead ablation | `src/bench_bilateral_upsampling.cpp::runProductionSteadyStateCase` | steady public bench | `0.93x / 0.91x / 0.97x` | 仍归属 old helper | phase 030 degradation | attempted / negative | 说明 public shell 不是旧 family 负向主因 |
| local nan-mask k64 | `src/bench_bilateral_upsampling.cpp::performProcessingNanMaskK64RVV` | local nan-mask k64 bench | `0.98x / 0.92x / 1.01x` | `vlse32/vmfeq/vmerge/vfmul/vfredusum` | phase 050 `Errors=2`、`Warnings=4`、`Suggestions=1` | attempted / negative | NaN-only，不覆盖 infinity，不能接 production |
| color-gather precursor | `src/bench_bilateral_upsampling.cpp::performProcessingColorGatherRVV` | production detail color-gather helper bench | `1.31x / 1.28x / 1.07x` | `vlse8/vzext/vmaxu/vminu/vluxei16/vfredusum` | phase 060 `Errors=0` | accepted precursor | bench-local helper，不是最终 adoption 证据 |
| color-gather production helper | `surface/include/pcl/surface/impl/bilateral_upsampling.hpp::bilateralUpsamplingPerformProcessingColorGatherRVV` | production public / steady public bench + public gtest | phase 073 public `1.26x / 1.21x / 1.18x / 1.23x / 1.22x`；steady `1.31x / 1.21x / 1.25x / 1.21x / 1.21x` | production helper 有 `vlse8/vzext/vmaxu/vminu/vluxei16/vfabs/vmflt/vfredusum` | phase 073 `Errors=0`、`Warnings=0`、`Suggestions=0` | adopted / refreshed | 覆盖 RGB/RGBA exact family、`Scalar=float`、AoS float layout |

## 标量路径与 RVV 路径差异

| 阶段 | 标量路径 | 当前 RVV 路径 | 证据 |
| --- | --- | --- | --- |
| window traversal | 双层 `(x_w, y_w)` 扫描 | 仍保留同一窗口边界；`y_w` 用 VL chunk 推进 | boundary gtest |
| depth weight | 从预计算 depth matrix 标量取值 | `vlse32.v` 沿 Eigen 列 stride 加载 | asm + board |
| RGB distance / lookup | 每个邻居标量算 `abs(dr)+abs(dg)+abs(db)` 再查表 | `vlse8.v` 取 RGB，`vzext` 扩展，`vmaxu/vminu` 得绝对差，`vluxei16.v` 查 RGB 表 | phase 060/070 asm |
| finite-depth skip | `std::isfinite(z)` 后累加 | `vmfeq(z,z)` 过滤 NaN，`vfabs/vmflt` 过滤 infinity，并 merge 到 0 后规约 | correctness tests + asm |
| reduction | 标量 `sum` / `norm_sum` | `vfredusum.vs` 分别规约 `w*z` 与 `w` | asm + board |
| unprojection / output | 标量矩阵乘和 RGB copy | 仍为标量写回，保持 production 语义 | public-entry tests |

## 细粒度目标字典

当前没有 bench case-filter CLI；细粒度隔离通过固定 label 和 phase 文档完成。`table window` 对应 staged-window；`direct-depth` 对应 column-stride-depth；`production public` 与 `production steady public` 对应真实公开入口；`production detail helper` 是兼容历史 helper-only label 的当前 color-gather 直连 smoke，`local nan-mask k64` 和 `production detail color-gather` 对应 helper 级 A/B 或 component ablation。

## 结论边界

当前 adopted 覆盖 `PointXYZRGB -> PointXYZRGB`、`PointXYZRGBA -> PointXYZRGBA`、`PointXYZRGB -> PointXYZRGBA`、`PointXYZRGBA -> PointXYZRGB` 的 organized RGBD grid、`Scalar=float` 和 `RVVXYZAoSFloatLayout`。`PointXYZ`、PointNormal-like 点型、`Scalar=double`、其它 layout、indices / correspondences 或非 organized 输入都没有被本证据链扩展；这些方向需要新 phase，并重新补 correctness、production direct bench、asm 和 Evidence Doctor。
