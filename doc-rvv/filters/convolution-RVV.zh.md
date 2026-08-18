# `convolution`（`impl/convolution.hpp`）：RVV 优化实现说明

本文说明 `filters/include/pcl/filters/impl/convolution.hpp` 中 `Convolution<PointXYZI, PointXYZI>` dense organized 路径的 `__RVV10__` 扩展。当前覆盖行方向 `convolve_rows` 与列方向 `convolve_cols`，同时处理 `x`、`y`、`z`、`intensity` 四个 float 字段，并覆盖 ignore、duplicate、mirror 三种边界策略的 dense `PointXYZI` 路径。公开 API 不变；不满足范围的路径保持标量实现。列方向曾在旧实现中出现板卡 `0.36x` 回退，当前版本已重写为固定输出行、横向 VL chunk 的访存组织。

实现文件：`filters/include/pcl/filters/impl/convolution.hpp`。

筛选与测试材料：

- 模块二轮筛选：`doc-rvv/library-screening/filters/filters-function-evaluation-queue.zh.md`
- 函数级评估：`test-rvv/filters/convolution/convolution-evaluation.zh.md`
- 专项测试与 bench：`test-rvv/filters/convolution/`

## 1. 背景与需求

`convolution.hpp` 是 filters 模块首批高优先级文件。当前覆盖风险最低、可对拍、可解释的 organized dense float 字段路径：

- `Convolution<PointXYZI, PointXYZI>`；
- `input_->is_dense == true`；
- `BORDERS_POLICY_IGNORE`、`BORDERS_POLICY_DUPLICATE`、`BORDERS_POLICY_MIRROR`；
- organized cloud；
- 行方向和列方向内部有效区域；
- `x/y/z/intensity` 四字段同步卷积；
- std/RVV 专项测试和 bench 使用同一套输入对拍。

暂缓覆盖：

- non-dense；
- 非 `PointXYZI` 点类型；
- `PointXYZRGB` / `PointXYZRGBA` packed 颜色特化；
- RGB/RGBA 颜色插值；
- OMP 并行策略调整。

正确性边界：

- 可以保证的是当前声明覆盖范围内的语义等价：dense organized `Convolution<PointXYZI, PointXYZI>`、ignore/duplicate/mirror 边界策略、`x/y/z/intensity` 四个 `float` 字段、行/列主体卷积。
- 不满足覆盖范围的路径不会走 RVV helper，而是回到标量路径；因此 non-dense、RGB/RGBA、其他点类型和上游复杂边界语义不由本 RVV 路径承担。
- RVV helper 与标量路径按相同 kernel tap 顺序做逐项乘加，没有引入向量归约重排；专项测试用手写标量参考对拍 rows/cols 的三种边界策略，QEMU 和板卡 bench 的 checksum 也与 std 一致。
- `PointXYZI` 覆盖依据来自 header 泛型标量路径的整点运算语义、上游 `Convolution.convolveRowsXYZI` 的 intensity 检查，以及本目录专项 rows/cols 四字段对拍；`filters/src/convolution.cpp` 中的 `RGB` / `PointXYZRGB` 显式特化只说明 packed 颜色有独立语义，不能作为扩大 RVV 覆盖范围的依据。

暂缓原因：

- non-dense 路径涉及 finite 判断、距离阈值、权重归一和缺失值语义；
- RGB/RGBA 特化存在 packed 颜色、截断和插值语义，不能套用 `PointXYZI` float 字段模型；
- 列方向旧实现按同一列的连续行做 VL chunk，板卡上因大 row-stride 访存回退；当前已改为固定输出行并沿列坐标横向分块，ignore、duplicate、mirror 均已有新版板卡日志确认真实性能。

## 2. 与上游实现的差异

| 条目 | 原路径 | RVV 路径 |
| --- | --- | --- |
| dense ignore rows | 对每个输出点调用单点 row dense 卷积 | 按 VL chunk 同时处理多个输出点，kernel tap 内对四字段做向量乘加 |
| dense ignore cols | 对每个输出点调用单点 col dense 卷积 | 固定输出行，沿宽度做 VL chunk；每个 kernel tap 读取一行上的相邻列，避免 row-stride 向量访存 |
| non-dense | finite / distance / 权重归一标量处理 | 保持标量 |
| duplicate / mirror | 标量处理边界和主体 | RVV 计算 dense 内区，再按原标量顺序补写边界 |
| RGB/RGBA 特化 | 专门颜色语义 | 保持既有特化路径 |
| 小尺寸或非覆盖类型 | 标量路径 | RVV 未命中时回到标量 |

当前结构按 filters RVV 工作流保持：

- 常驻原标量路径；
- `__RVV10__` 下提供 `PointXYZI` 专用 RVV helper；
- 行/列入口只做窄范围短路分流；
- RVV helper 命中后完成 dense 内区；ignore 边界由 helper 写 NaN，duplicate/mirror 边界由入口标量补写；
- 未命中范围保持原路径，避免影响泛型模板和 RGB 特化。
- 字段级 RVV load/store 使用公共 `pcl/rvv_point_load.h` 与 `pcl/rvv_point_store.h` 的 PCL field-tag helper；traits 只验证被访问字段是注册的单个 `float` 字段，不替代本算法的 exact `PointXYZI -> PointXYZI` dispatch。

## 3. 总体设计

`PointXYZI` 是 AoS 点布局，当前不重新组织数据，只按字段地址做 strided load/store。RVV 路径的处理模型是：

1. 确认 `Convolution<PointXYZI, PointXYZI>`；
2. 确认 input dense；
3. 行方向固定行 `r`，沿列 `c` 分块；
4. 列方向同样固定输出行 `r`，沿列 `c` 分块；每个 kernel tap 读取相邻源行上的同一批列；
5. 每个 VL chunk 初始化四个输出累加向量；
6. 遍历 kernel tap，将对应输入四字段乘以 kernel 系数并累加；
7. 写回输出四字段；
8. ignore 边界写 NaN；duplicate/mirror 复用 RVV 内区结果并按原标量顺序补写边界；
9. non-dense、RGB/RGBA 和其他点类型留给原路径。

该路径只改变覆盖范围内的主体计算方式，不改变公开 API、输出尺寸、边界策略或点类型分发。

## 4. 数值算例与访存图示

以下只展示一个字段，`y/z/intensity` 与 `x` 完全同构。为避免 `(i,j)` 的“行/列”歧义，本文使用 PCL `PointCloud` 的访问约定：

```text
cloud(c, r) == cloud(column, row)
c 表示列，向右增长；r 表示行，向下增长。
```

令 kernel 为 `[A, B, C]`，PCL 标量循环中 `kernel_width = 2`、`half_width = 1`，内部 tap 顺序是：

```text
out = input[left/up] * C + input[center] * B + input[right/down] * A
```

### 4.1 行方向

行方向 `convolve_rows` 固定一行 `r`，沿列 `c` 做卷积。下面用一个 `4 x 4` 网格说明，当前只看第 `r=1` 行：

```text
          c=0  c=1  c=2  c=3
r=0       01   02   03   04
r=1       10   20   30   40   <- 固定这一行做 rows
r=2       50   60   70   80
r=3       90   91   92   93
```

ignore 边界下，`c=0` 和 `c=3` 输出 NaN；内区 `c=1,2` 的标量结果是：

```text
out(c=1,r=1) = cloud(0,1)*C + cloud(1,1)*B + cloud(2,1)*A
             = 10*C + 20*B + 30*A

out(c=2,r=1) = cloud(1,1)*C + cloud(2,1)*B + cloud(3,1)*A
             = 20*C + 30*B + 40*A
```

RVV 行方向在 `c=1`、`vl=2` 时把相邻输出列 `[1, 2]` 放进同一个向量 chunk：

```text
tap C: [10, 20] * C
tap B: [20, 30] * B
tap A: [30, 40] * A
acc  : [10*C + 20*B + 30*A,
        20*C + 30*B + 40*A]
```

这与两个标量输出逐项一致。由于 `PointXYZI` 是 AoS，RVV 使用字段 stride `sizeof(PointXYZI)` 读取同一字段的相邻点：

```text
内存点序: P0        P1        P2        P3
字段 x : P0.x  ... P1.x  ... P2.x  ... P3.x
RVV x  : strided_load_field_f32m2<PointXYZI, fields::x>(base=&P0)
```

### 4.2 列方向

列方向 `convolve_cols` 的数学语义是固定一列 `c`、沿行 `r` 取上/中/下邻域。但新 RVV 实现并不沿同一列做 VL chunk，而是固定输出行 `r`，横向处理多个列 `c`。这样每个 tap 仍来自上/中/下三行，只是一次处理多个相邻列。

旧列方向实现曾把“同一列的连续行”作为 VL chunk：

```text
lane0 -> cloud(c, r)
lane1 -> cloud(c, r+1)
lane2 -> cloud(c, r+2)

lane 间地址步长 = width * sizeof(PointXYZI)
```

这在板卡上导致大 stride 向量访存，旧日志中 `640x240-k7-cols` 只有约 `0.36x`。当前实现改为固定输出行 `r`，沿列 `c` 做 VL chunk。继续用一个 `4 x 4` 网格说明，当前计算输出行 `r=1` 的列 `[1,2]`：

```text
          c=0  c=1  c=2  c=3
r=0       01   10   20   04   <- tap C 来源行 r-1
r=1       05   30   40   08   <- tap B 来源行 r
r=2       09   50   60   12   <- tap A 来源行 r+1
r=3       13   14   15   16
                ^    ^
              c=1  c=2，vl=2
```

标量列卷积分别是：

```text
out(c=1,r=1) = cloud(1,0)*C + cloud(1,1)*B + cloud(1,2)*A
             = 10*C + 30*B + 50*A

out(c=2,r=1) = cloud(2,0)*C + cloud(2,1)*B + cloud(2,2)*A
             = 20*C + 40*B + 60*A
```

RVV 横向 chunk 对这两个列同时计算：

```text
tap C 读取上一行相邻列: [cloud(1,0), cloud(2,0)] = [10, 20]
tap B 读取当前行相邻列: [cloud(1,1), cloud(2,1)] = [30, 40]
tap A 读取下一行相邻列: [cloud(1,2), cloud(2,2)] = [50, 60]

acc = [10*C + 30*B + 50*A,
       20*C + 40*B + 60*A]
```

这正是 `out(c=1,r=1)` 与 `out(c=2,r=1)` 的标量列卷积结果。关键点是：数学上仍是“列卷积”，但向量 lanes 选择横向相邻列；每个 tap 换一行读取同一批列。lane 间只跨相邻列的 `sizeof(PointXYZI)` 字段 stride，避免旧 row-stride 组织。

上下边界行 `r=0` 和 `r=height-1` 不交给 RVV 主体。ignore 策略写 NaN；duplicate/mirror 策略则在 RVV 主体完成后按原标量循环从内区复制边界。

### 4.3 duplicate / mirror 边界

duplicate 和 mirror 的主体卷积与 ignore 完全相同，差别只在边界写法。当前实现让 RVV helper 支持 `fill_ignore_borders` 参数：

- ignore：`fill_ignore_borders=true`，helper 计算内区并写 NaN 边界；
- duplicate/mirror：`fill_ignore_borders=false`，helper 只计算内区，入口再用原标量循环补写边界。

这里不能对 duplicate/mirror 先写 NaN 再覆盖边界。原因是上游 mirror 标量实现有既有写入顺序，例如行方向左边界：

```text
for c = 0..half_width-1:
  output(c,r) = output(half_width + 1 - c, r)
```

当 `half_width=3` 时，`c=2` 会读取 `output(2,r)` 自身。标量路径中该位置来自输出云的默认值；如果 RVV helper 已经把 ignore 边界写成 NaN，就会改变 mirror 语义。当前 duplicate/mirror 入口因此只让 RVV 写内区，边界保持由原顺序决定。

## 5. 验证结果

### 5.1 QEMU 功能验证

命令：

```bash
make -C test-rvv/filters/convolution run_test_compare
```

结果：std 与 RVV 两套 `test_convolution` 二进制均通过 8 个专项用例。专项测试覆盖：

- dense ignore rows，与手写标量参考对拍；
- dense ignore cols，与手写标量参考对拍；
- dense duplicate rows/cols，与手写标量参考对拍；
- dense mirror rows/cols，与手写标量参考对拍，并覆盖 mirror 自拷贝边界语义；
- non-dense / fallback 路径保持语义，不误入 dense RVV 参考。

### 5.2 QEMU bench 与指令路径

命令：

```bash
make -C test-rvv/filters/convolution run_bench_compare dump_bench_rvv
```

QEMU bench 只作为构建、运行、日志格式和指令路径证据，不作为真实性能结论。当前 `output/qemu/analyze_bench_compare.log` 已能解析：

- `Dataset: organized dense PointXYZI convolution qemu-smoke; ignore baseline plus duplicate/mirror rows/cols cases`；
- `Iterations: 5`；
- `Total Time = Avg × 5`，不再出现 `n/a`。

当前 QEMU smoke 覆盖：

- ignore 基线：`128x64-k7-rows`、`128x64-k7-cols`、`256x128-k15-rows`；
- 新增边界策略：`128x64-k7-rows-duplicate`、`128x64-k7-cols-duplicate`、`128x64-k7-rows-mirror`、`128x64-k7-cols-mirror`。

QEMU smoke 中不同 case 的快慢不作为硬件性能判断。

已确认 RVV bench 二进制中存在目标指令路径：

- `vsetvli`；
- `vlse32.v`；
- `vsse32.v`；
- `vfmacc.vv`；
- `vfmv.v.f`。

反汇编产物：`test-rvv/filters/convolution/build/asm/riscv/bench_convolution_rvv.asm`。

### 5.3 板卡状态

板卡入口已具备：

```bash
make -C test-rvv/filters/convolution deploy_board run_board_test run_board_bench_compare fetch_board_logs
```

已有新版板卡日志已保存到 `test-rvv/filters/convolution/output/board/bench_compare.log`，该日志覆盖 ignore 基线与 duplicate/mirror 新增 case，结果如下：

- `640x240-k7-rows`：Std `32.9047 ms/iter`，RVV `11.0425 ms/iter`，约 `2.98x`；
- `640x240-k7-cols`：Std `57.4873 ms/iter`，RVV `15.9706 ms/iter`，约 `3.60x`；
- `1280x480-k15-rows`：Std `230.0258 ms/iter`，RVV `64.8247 ms/iter`，约 `3.55x`；
- `640x240-k7-rows-duplicate`：Std `32.5289 ms/iter`，RVV `11.3815 ms/iter`，约 `2.86x`；
- `640x240-k7-cols-duplicate`：Std `57.4468 ms/iter`，RVV `14.9296 ms/iter`，约 `3.85x`；
- `640x240-k7-rows-mirror`：Std `32.5159 ms/iter`，RVV `11.4035 ms/iter`，约 `2.85x`；
- `640x240-k7-cols-mirror`：Std `57.6554 ms/iter`，RVV `15.0609 ms/iter`，约 `3.83x`。

此前旧版列方向曾出现 `640x240-k7-cols` 约 `0.36x`。原因是旧 helper 以同一列的连续行作为 VL chunk，lane 间 load/store 步长为 `width * sizeof(PointXYZI)`。这类大 stride 访存在板卡上成本很高，抵消了卷积乘加的收益。

当前实现已改为固定输出行、横向 VL chunk，新版板卡结果显示 ignore 列方向约 `3.60x`，duplicate/mirror 列方向约 `3.85x` / `3.83x`。duplicate/mirror 的边界补写是小规模标量 copy，板卡结果显示整体收益仍主要来自 RVV 内区主体。

### 5.4 上游原始测试

已完成：

```bash
make -C test-rvv/filters/convolution run_upstream_test_compare
```

`test-rvv/filters/convolution/Makefile` 已按交叉编译指南和既有 `plane_models` / `centroid` 专项补齐上游测试依赖链：`pcl_sample_consensus`、`pcl_search`、`pcl_kdtree`、`pcl_octree`、`flann_cpp`、`lz4`、`hdf5`、`zlib`、`libpng` 与 Boost filesystem/iostreams/system，并把对应 lib 目录加入 `LIB_DIRS_LIST`、`LDFLAGS` 与 QEMU `LD_LIBRARY_PATH`。

结果：std 与 RVV 两套上游 `test_convolution` 二进制在 QEMU 下均通过 3 个上游用例：

- `Convolution.convolveRowsXYZI`；
- `Convolution.convolveRowsRGB`；
- `Convolution.convolveRowsXYZRGB`。

## 6. 当前结论

| 项目 | 状态 |
| --- | --- |
| 函数级评估 | 完成 |
| RVV 实现 | 完成当前范围 |
| 专项测试 | QEMU std/RVV 对拍通过 |
| QEMU bench | 通过，输出可解析 |
| 反汇编证据 | 已确认 RVV 指令路径 |
| 板卡入口 | 已具备；ignore / duplicate / mirror 板卡 bench 已完成 |
| 上游测试 | QEMU std/RVV 对拍通过 |

`convolution` 当前已完成代码、专项测试、QEMU bench、反汇编证据、旧列方向回退复盘、ignore / duplicate / mirror 板卡性能验证和上游原始测试对拍。
