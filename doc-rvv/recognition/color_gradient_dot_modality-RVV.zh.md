# ColorGradientDOTModality RVV 生产接入说明

## 当前状态

`ColorGradientDOTModality<PointInT>::processInputData()` 已在 RVV（RISC-V Vector，RISC-V 向量扩展）构建下接入
`computeMaxColorGradientsRVV()`。该路径接管 organized RGB 输入的梯度图生成；`computeDominantQuantizedGradients()`
仍保留原标量实现，负责每个 bin 内最大 magnitude（幅值）选择、方向量化和空 bin 标记。

本生产结论来自 production direct（真实生产路径）板卡证据：
`test-rvv/recognition/color_gradient_dot_modality/log/board/repeated_phase010_production_direct/summary.md`。
Evidence Doctor（证据体检）结果为 Errors=0、Warnings=0。

## 生产性能

| case | timer boundary | runs | median speedup | p10 - p90 | B/A < 1 | checksum |
| --- | --- | ---: | ---: | ---: | ---: | --- |
| `process_input_320x240` | `ColorGradientDOTModality<PointXYZRGB>::processInputData()` | 5 | `2.600x` | `2.568x` - `2.628x` | 0/5 | `16942614990468929485` |
| `process_input_641x481_tail` | `ColorGradientDOTModality<PointXYZRGB>::processInputData()` | 5 | `3.510x` | `3.484x` - `3.516x` | 0/5 | `6040139219709186475` |

该 timer boundary（计时边界）包含 production `computeMaxColorGradients` / `computeMaxColorGradientsRVV`
分流和原有 dominant bin 标量扫描。QEMU（仿真器）只用于 correctness（正确性）、构建和日志形状，不作为性能依据。

## 标量语义

原标量路径对每个 `(row, col)` 读取当前像素、右侧两列像素和下方两行像素的 RGB 值。三通道分别计算
`dx`、`dy` 和 `dx*dx + dy*dy`，通道选择规则是：

1. 红色只有在严格大于绿色和蓝色时胜出。
2. 绿色只有在红色不胜出且严格大于蓝色时胜出。
3. 其它情况选择蓝色。

被选通道的 `sqrt` 和 `atan2` 写入 `color_gradients_(col + 1, row + 1)`，`GradientXY::x/y` 保留原始
`col/row`。后续 `computeDominantQuantizedGradients()` 对每个 `bin_size_ x bin_size_` 区域用 strict `>`
寻找最大 magnitude，达到阈值后写 one-hot direction bit，否则写 `1 << 7`。

## RVV 实现

`computeMaxColorGradientsRVV()` 使用 `vlse8` 从 `PointInT` 的 R/G/B 字段跨步读取字节，扩宽到 32-bit integer。
每个 VL chunk（向量长度分块）内，RVV 批量计算右向和下向差分、三个通道的平方幅值、通道选择 mask、
`vfsqrt` 和 `pcl::atan2_RVV_f32m2`。角度转为 degree（角度制）后写回 `GradientXY` 状态。

RVV helper 不改 dominant bin 扫描顺序。这样生产补丁只替换像素级梯度数学热点，保留原 bin 内最大值 tie-break、
阈值和输出 byte map 语义。

## 当前采用的优化方式

| 维度 | 当前状态 | 采用或暂缓原因 | 证据 | 边界 / 下一步 |
| --- | --- | --- | --- | --- |
| gradient map | adopted | RGB 差分、最大通道选择、`sqrt` 和 `atan2` 是逐像素热点，RVV 后 production direct 稳定正向。 | Phase 010 board summary | 当前只覆盖 `PointXYZRGB` organized cloud。 |
| dominant bin scan | scalar tail | strict `>` tie-break 和 bin 内状态选择直接决定输出 byte，继续保持原标量顺序最稳。 | correctness 对拍 | 若未来 profile 指向该段，再另开 phase。 |
| invariant map | deferred | 会临时把 `color_gradients_` 的 magnitude 改成 `-1` 再恢复，状态风险高。 | 当前未取证 | template creation profile 指向该路径时再评估。 |

一个 VL chunk 的手工对齐示例：假设某一行从 `col=0` 开始，当前 `vl=4`，RVV 会同时处理
`col=0..3` 对应的四个中心点。每个 lane（向量通道）分别读取中心点、右侧 `col+2` 点和下方 `row+2` 点的
R/G/B 字段，计算三组 `dx*dx + dy*dy`。如果 lane 0 中蓝色幅值最大，lane 0 的输出就使用蓝色的
`dx/dy` 计算 magnitude 和 angle，并写到 `color_gradients_(1, row+1)`；后续 lane 按相同规则写到
`color_gradients_(2..4, row+1)`。这与标量循环“读 `(row,col)`，写 `(row+1,col+1)`”的位移关系一致。

## 覆盖与回退

| scope | status | evidence |
| --- | --- | --- |
| 非 RVV 构建 | fallback | `processInputData()` 调用原 `computeMaxColorGradients()`。 |
| `width < 3 || height < 3` | fallback-equivalent | RVV helper 只 resize state 后返回，与原循环无迭代一致。 |
| organized `PointXYZRGB` | adopted | correctness、asm 和 production direct board 覆盖。 |
| 其它 `PointInT` | compile-shape only | 模板仍按字段访问编译；没有独立 correctness / board 矩阵，不外推为已证明。 |
| `computeInvariantQuantizedMap()` | not covered | 该路径会临时改写并恢复 `color_gradients_`，需要独立状态恢复测试。 |
| DOTMOD template matching | not covered | `recognition/src/dotmod.cpp` 是独立 topic。 |

## 正确性与高效性证据链

| evidence | path | result |
| --- | --- | --- |
| correctness | `make -C test-rvv/recognition/color_gradient_dot_modality run_test_compare` | Std/RVV 3 个 gtest 通过，production public entry 输出与 scalar reference 一致。 |
| QEMU smoke | `run_bench_rvv BENCH_ARGS="--case-filter process_input_320x240 --iterations 1 --warmup-iterations 1"` | 只证明可运行和日志形状。 |
| asm | `make -C test-rvv/recognition/color_gradient_dot_modality check_cgdm_rvv_asm` | RVV 指令归属到 `bench_cgdm_rvv` 中的 production helper 链路。 |
| board | `log/board/repeated_phase010_production_direct/summary.md` | 两个 public entry case 5-run stable positive。 |
| Evidence Doctor | `log/board/repeated_phase010_production_direct/evidence_doctor.md` | Errors=0，Warnings=0，Suggestions=4。 |

## Traceability Map

| item | role | path |
| --- | --- | --- |
| production entry | `ColorGradientDOTModality<PointInT>::processInputData()` | `recognition/include/pcl/recognition/color_gradient_dot_modality.h` |
| scalar helper | `computeMaxColorGradients()` | `recognition/include/pcl/recognition/color_gradient_dot_modality.h` |
| RVV helper | `computeMaxColorGradientsRVV()` | `recognition/include/pcl/recognition/color_gradient_dot_modality.h` |
| dominant map helper | `computeDominantQuantizedGradients()` | `recognition/include/pcl/recognition/color_gradient_dot_modality.h` |
| test / bench assets | correctness and production direct board wrapper | `test-rvv/recognition/color_gradient_dot_modality/` |
| phase closeout | production integration result | `test-rvv/recognition/color_gradient_dot_modality/doc/phases/010-production-integration/result.zh.md` |

## Production closeout

| file | helper / entry | status | 回滚边界 |
| --- | --- | --- | --- |
| `recognition/include/pcl/recognition/color_gradient_dot_modality.h` | `processInputData()` dispatch | adopted | 移除 `__RVV10__` 分支即可回到纯标量路径。 |
| `recognition/include/pcl/recognition/color_gradient_dot_modality.h` | `computeMaxColorGradientsRVV()` | adopted | 只在 RVV 构建下编译，不影响非 RVV 构建。 |
| `test-rvv/recognition/color_gradient_dot_modality/` | correctness / bench / evidence | retained | 作为 production direct 回归和证据复核入口保留。 |

## 后续条件

当前 topic 可收口。若后续 profile（剖析）显示 template creation 仍由 invariant map 搜索主导，应新开
`cgdm-invariant-map-rvv` phase，并覆盖 `MaskMap`、`RegionXY`、状态恢复和板卡 production-shaped diagnostic。
