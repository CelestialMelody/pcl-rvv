# bilateral_upsampling 优化路线图

## 当前边界

当前主题只覆盖 `BilateralUpsampling::performProcessing` 的 organized 像素网格、有界窗口、RGB 查表权重、finite depth skip（有限深度跳过）和输出 unprojection（反投影）。`process` 中的 `projection_matrix_.inverse()` 和 `computeDistances` 的 `std::exp` 预计算不是首要热点；它们每次 process 只执行一次。

## 候选搜索空间

| candidate family | idea source | applies to | expected benefit | risk / unknown | required evidence | status | next phase |
| --- | --- | --- | --- | --- | --- | --- | --- |
| staged-window-reduction | 当前源码窗口累加 | 窗口 weight/depth 乘法和规约 | 降低窗口累加成本，保持标量查表语义 | 额外 staging 已被板卡证明抵消收益；phase 000 为 0.90x / 0.91x / 0.90x | correctness、asm、board、Evidence Doctor | rejected | 不接 production |
| column-stride-depth-direct | 当前 organized layout | 对固定 `x_w`、连续 `y_w` 使用 strided depth load | 减少 depth staging；更接近 production AoS 访问 | RGB weight 仍标量暂存；phase 010 diagnostic board 为 1.04x / 1.16x / 1.28x，但旧 production family 曾负向 | correctness、asm、board、Evidence Doctor | historical precursor | 已被 color-gather family 取代为当前主线 |
| production PointXYZRGB/RGBA exact gate | 生产源码预编译点型 | `PointXYZRGB` / `PointXYZRGBA` | 真实公开入口收益 | historical public 为 0.93x / 0.90x / 0.97x；steady-state 为 0.93x / 0.91x / 0.97x | public direct tests、asm、board、Evidence Doctor | rejected / superseded | 保留作历史对照 |
| production detail helper-only | phase 040 归因 | 当前 production helper 本体 | 若 public shell 是主因，helper-only 应稳定正向 | helper-only 在 phase 040 两次 bounded run 中由 1.07x / 1.02x / 1.01x 翻到 0.94x / 0.90x / 0.95x，随后 rerun 为 0.91x / 0.91x / 0.95x | helper-only board、Evidence Doctor | attempted / unstable-negative | 不建议继续同一 helper |
| production detail local nan-mask k64 | phase 050 组件消融 | 测试专用生产点型 helper | 判断 strict finite mask / chunk 形态是否是主因 | 只覆盖 NaN，不覆盖 infinity；phase 050 board 为 0.98x / 0.92x / 1.01x | QEMU smoke、asm、board、Evidence Doctor | attempted / negative | 不进入 production；除非用户指定继续新候选族 |
| production detail color-gather | phase 060/070 阶段反思 | 把 RGB 距离和 RGB 查表挪到 RVV lane 内，并接入 production helper | 消除旧 helper 的标量颜色 staging 开销 | phase 073 当前 public / steady 均为正向，最低 public speedup 为 1.18x | QEMU correctness、asm、production public/steady board、Evidence Doctor、用户确认 | adopted / refreshed | 当前采用的 production family |
| production finite-mask correctness refresh | closeout 审计 | color-gather helper 的 finite-depth mask | 对齐标量 `std::isfinite`，跳过 NaN 和 infinity | 新增 `vfabs/vmflt` 后已经由 phase 073 刷新 board | QEMU correctness、asm、board_smoke | adopted correctness refresh | 已完成 |
| production same-type gate alignment | closeout 审计 | production RVV gate | 让源码启用范围与测试、bench manifest 和板卡证据一致 | phase 072 先保守回退交叉点型；phase 073 已补交叉证据 | QEMU correctness、asm、board_smoke | historical scope alignment | 已被 phase 073 扩展 |
| cross RGB/RGBA production probe | phase 073 点类型扩展 | `PointXYZRGB -> PointXYZRGBA` 与 `PointXYZRGBA -> PointXYZRGB` public entry | 消除 phase 072 的交叉点型 fallback 缺口 | phase 073 cross public 为 1.23x / 1.22x，steady 为 1.21x / 1.21x | QEMU correctness、asm、board_smoke、Evidence Doctor | adopted scope expansion / refreshed | 后续其它点型另开 phase |

## 暂缓 / 拒绝路线

| candidate family | reason | resume condition |
| --- | --- | --- |
| RVV 化 `computeDistances` 的 `std::exp` | 该表每次 process 只生成一次，不在逐像素热点循环内。 | profile 证明预计算异常占比时再复查。 |
| current production exact-gate helper family | public、steady public、helper-only 和 nan-mask/chunk 消融都没有稳定正向；historical production public 为 0.93x / 0.90x / 0.97x，phase 040 helper-only rerun 为 0.91x / 0.91x / 0.95x。 | 已被 color-gather family 取代为当前主线，保留作历史对照。 |

## 推荐 PI1 边界

phase 020 已经完成 `column-stride-depth-direct` 的生产探针，phase 030/040/050 又补了 public shell、helper-only 和 mask/chunk 消融。phase 060/070 切换到 color-gather family 后，same-type production public / steady board 已转正，并已被用户确认保留。phase 071/072 分别补 finite-mask correctness 和 same-type gate alignment；phase 073 又补交叉 RGB/RGBA production probe，并刷新当前二进制 QEMU/asm/board 证据，Evidence Doctor 为 `Errors=0`、`Warnings=0`、`Suggestions=0`。下一步只在需要其它点型、layout 或 `Scalar` 扩展时另开 point-type / layout expansion phase。
