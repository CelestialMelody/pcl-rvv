# `filters/include/pcl/filters/impl/convolution.hpp`：函数级评估与 RVV 实施记录

本文档记录 `convolution` 主题的函数级筛选、RVV 覆盖范围、验证证据和暂缓项。该主题来自 filters 模块第二轮筛选。当前实现已按板卡旧日志中列方向 `0.36x` 的问题重写列方向 RVV 的 VL 组织，并补充 dense `PointXYZI` duplicate/mirror 边界策略覆盖；已完成 QEMU 对拍、QEMU bench 格式检查、反汇编证据和 ignore / duplicate / mirror 新版板卡验证。

## 1. 函数 / 函数组梳理

| 函数 / 函数组 | 功能概要 | 复杂度与访存 | 当前判断 |
| --- | --- | --- | --- |
| `initCompute` | 校验 kernel、border policy、输出尺寸 | O(1) | 不做 RVV |
| `convolveRows` / `convolveCols` / `convolve` | 公开批量入口和两阶段行列卷积分发 | 包装 + 调用内部行列路径 | 自身不做 RVV，随内部路径受益 |
| `convolveOneRowDense` | 单点行方向 kernel 加权 | O(K)，同一行连续邻域 | 保持标量；已改为外层 dense 行路径批处理 |
| `convolveOneColDense` | 单点列方向 kernel 加权 | O(K)，跨行 stride 访问 | 保持标量；已改为外层 dense 列路径批处理 |
| `convolveOneRowNonDense` / `convolveOneColNonDense` | 带 finite 和距离阈值的加权 | O(K)，分支、距离判断、权重归一 | 暂缓 |
| `convolve_rows` | 遍历 organized cloud 每行，处理边界并调用 dense/non-dense 单点卷积 | O(width * height * K) | 已覆盖 dense `PointXYZI` + ignore boundary RVV 路径 |
| `convolve_cols` | 遍历每列，处理边界并调用 dense/non-dense 单点卷积 | O(width * height * K)，原单点语义跨行取 tap | 已覆盖 dense `PointXYZI` + ignore boundary RVV 路径；当前按固定输出行、横向 VL chunk 重写 |
| `convolve_rows_duplicate/mirror` / `convolve_cols_duplicate/mirror` | 不同边界策略 | 同上，边界赋值语义不同 | 已覆盖 dense `PointXYZI` 主体；边界按原标量顺序补写 |
| `makeInfinite` 与 RGB / PointXYZRGB / PointXYZRGBA 特化 | 边界输出和颜色处理 | 小规模或类型特化 | 不覆盖，保持既有语义 |

## 2. 覆盖范围

已实现范围：

- 点类型：`Convolution<PointXYZI, PointXYZI>`；
- 输入形态：organized point cloud；
- 输入语义：`input_->is_dense == true`；
- 边界策略：`BORDERS_POLICY_IGNORE`、`BORDERS_POLICY_DUPLICATE`、`BORDERS_POLICY_MIRROR`；
- 方向：`convolve_rows` 与 `convolve_cols`；
- 字段：`x` / `y` / `z` / `intensity` 四个 float 字段同步卷积；
- 宏：沿用项目现有 `__RVV10__`；
- fallback：不满足覆盖条件时保持原标量路径。

明确不覆盖：

- non-dense 输入；
- 非 `PointXYZI` 点类型；
- `PointXYZRGB` / `PointXYZRGBA` packed 颜色特化；
- RGB/RGBA 颜色插值路径；
- distance threshold 相关 non-dense 加权归一语义；
- OMP 并行策略调整。

## 3. 实施结构

当前结构保持公开 API 不变，只在既有行/列内部路径中增加窄范围短路：

1. 进入行或列方向内部处理；
2. 判断是否为 dense input、`PointXYZI -> PointXYZI`；
3. 命中时进入 RVV helper；
4. RVV helper 按 VL chunk 处理多个输出点；
5. 每个 kernel tap 读取输入四个 float 字段并做乘加；
6. 输出四个字段；
7. ignore 策略由 helper 写 NaN 边界；duplicate/mirror 策略只用 helper 计算内区，再按原标量顺序补写边界；
8. 未命中或不在覆盖范围内时回到原标量路径。

行方向和列方向都使用 AoS 字段 stride 访问模型，但 VL 组织不同：

- 行方向：同一行内相邻输出点形成规整滑窗；
- 列方向：按 PCL `cloud(column, row)` 约定，固定输出行 `r`，沿列 `c` 分块；每个 kernel tap 读取相邻源行上的同一批列，避免旧实现的 `width * sizeof(PointXYZI)` 大步长向量 load/store；
- 两个方向的 RVV 主体都只处理有效内区；ignore 策略由 helper 写 NaN 边界，duplicate/mirror 策略复用同一内区结果并标量补写边界。

## 4. 专项测试与 bench

专项目录：`test-rvv/filters/convolution/`。

| 文件 | 作用 |
| --- | --- |
| `test_convolution.cpp` | dense rows / dense cols ignore、duplicate、mirror 与手写标量参考对拍；non-dense 回退语义检查 |
| `bench_convolution.cpp` | QEMU smoke bench 与板卡完整数据集入口；覆盖 ignore 基线和 duplicate/mirror rows/cols |
| `Makefile` | std/RVV 构建、QEMU test/bench、反汇编、上游测试入口、板卡入口 |
| `board.mk` | 板卡部署、运行、日志拉回入口 |

专项单测已通过：

```bash
make -C test-rvv/filters/convolution run_test_compare
```

结果：std 与 RVV 两套 RISC-V 二进制在 QEMU 下通过 8 个专项用例。覆盖 dense rows/cols 的 ignore、duplicate、mirror，small fallback，以及 non-dense 回退语义检查。

补充验证点：mirror 策略不能先写 ignore NaN 边界再补镜像，因为原标量实现中靠近内区的一格会按既有输出内容自拷贝。当前 RVV duplicate/mirror 入口传入 `fill_ignore_borders=false`，只计算内区，再按原标量循环顺序补边界；专项测试已覆盖该语义。

## 5. QEMU bench 与反汇编证据

已完成命令：

```bash
make -C test-rvv/filters/convolution run_bench_compare dump_bench_rvv
```

已生成 bench 分析日志：

- `test-rvv/filters/convolution/output/qemu/analyze_bench_compare.log`

当前 QEMU smoke 数据集：

- `128x64-k7-rows`；
- `128x64-k7-cols`；
- `256x128-k15-rows`；
- `128x64-k7-rows-duplicate`；
- `128x64-k7-cols-duplicate`；
- `128x64-k7-rows-mirror`；
- `128x64-k7-cols-mirror`；
- `Iterations: 5`。

QEMU bench 已能被 `test-rvv/script/analyze_bench_compare.py` 解析，未发现 `未解析`、`n/a`、`Total Time 不计算`。QEMU 下不同 case 的快慢不作为真实性能结论；这里只作为构建、运行、格式和指令路径证据。

反汇编文件：

- `test-rvv/filters/convolution/build/asm/riscv/bench_convolution_rvv.asm`

已确认存在 RVV 指令证据：

- `vsetvli`；
- `vlse32.v`；
- `vsse32.v`；
- `vfmacc.vv`；
- `vfmv.v.f`。

## 6. 板卡入口状态

已检查板卡入口展开：

```bash
make -C test-rvv/filters/convolution -n deploy_board run_board_test run_board_bench_compare fetch_board_logs
```

已有新版板卡日志位于 `test-rvv/filters/convolution/output/board/bench_compare.log`。该日志覆盖 ignore 基线与 duplicate/mirror 新增 case，结果：

- `640x240-k7-rows`：Std `32.9047 ms/iter`，RVV `11.0425 ms/iter`，约 `2.98x`；
- `640x240-k7-cols`：Std `57.4873 ms/iter`，RVV `15.9706 ms/iter`，约 `3.60x`；
- `1280x480-k15-rows`：Std `230.0258 ms/iter`，RVV `64.8247 ms/iter`，约 `3.55x`；
- `640x240-k7-rows-duplicate`：Std `32.5289 ms/iter`，RVV `11.3815 ms/iter`，约 `2.86x`；
- `640x240-k7-cols-duplicate`：Std `57.4468 ms/iter`，RVV `14.9296 ms/iter`，约 `3.85x`；
- `640x240-k7-rows-mirror`：Std `32.5159 ms/iter`，RVV `11.4035 ms/iter`，约 `2.85x`；
- `640x240-k7-cols-mirror`：Std `57.6554 ms/iter`，RVV `15.0609 ms/iter`，约 `3.83x`。

此前旧列方向 RVV 以同一列的连续行作为 VL chunk，lane 间访存步长为 `width * sizeof(PointXYZI)`，板卡上大 stride load/store 成本超过乘加收益，导致旧 `640x240-k7-cols` 约 `0.36x`。当前实现已改为固定输出行、沿列坐标横向分块，新版板卡结果显示 ignore 列方向约 `3.60x`，duplicate/mirror 列方向约 `3.85x` / `3.83x`。

真实性能结论以上述新版板卡日志为准。QEMU 结果仍只作为构建、运行、格式和指令路径证据。

## 7. 上游测试状态

已完成：

```bash
make -C test-rvv/filters/convolution run_upstream_test_compare
```

此前误判为 RISC-V 环境依赖缺口。复查 `doc-rvv/build/RISC-V PCL Cross-Compilation Guide.zh.md`、`test-rvv/sample_consensus/plane_models/Makefile` 和 `test-rvv/common/centroid/Makefile` 后，确认 `flann/lz4/hdf5/zlib/libpng/boost` 等依赖已经安装，问题是当前 `convolution` Makefile 未显式补齐 `libpcl_filters` 的依赖链。

已在 `test-rvv/filters/convolution/Makefile` 中补齐 include/lib 路径、`-Wl,-rpath-link`、QEMU `LD_LIBRARY_PATH` 与 `LIBS_UPSTREAM_TEST`，包含 `pcl_sample_consensus`、`pcl_search`、`pcl_kdtree`、`pcl_octree`、`flann_cpp`、`lz4`、`hdf5`、`zlib`、`libpng` 和 Boost filesystem/iostreams/system。

结果：std 与 RVV 两套上游 `test_convolution` 二进制在 QEMU 下均通过 3 个上游用例。

## 8. 当前结论

| 项目 | 状态 |
| --- | --- |
| 函数级评估 | 完成 |
| RVV 实现 | 完成 |
| 专项单测 | QEMU std/RVV 对拍通过 |
| QEMU bench | 通过，输出可解析 |
| 反汇编证据 | 已确认 RVV 指令路径 |
| 板卡入口 | 已具备；ignore / duplicate / mirror 板卡 bench 已完成 |
| 上游原始测试 | QEMU std/RVV 对拍通过 |
| 主题文档 | 已沉淀到 `doc-rvv/filters/convolution-RVV.zh.md` |

`convolution` 当前代码、专项验证、QEMU 证据、反汇编证据、ignore / duplicate / mirror 板卡性能结论和上游原始测试对拍均已更新。
