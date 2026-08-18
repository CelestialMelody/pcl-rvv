# filters/pyramid 函数级 RVV 评估

## 1. 目标与入口

本主题来自 `doc-rvv/library-screening/filters/filters-retained-candidate-rescreen.zh.md` 的“建议启动函数级评估”清单。目标入口是 `pcl::filters::Pyramid<PointT>::compute(std::vector<PointCloudPtr>&)`，实现文件为 `filters/include/pcl/filters/impl/pyramid.hpp`，RGB/RGBA/RGB 显式特化位于 `filters/src/pyramid.cpp`。

`Pyramid` 用于从 organized 点云构建多层尺度金字塔。`output[0]` 是输入点云副本，后续每层宽高各减半；每个输出点由上一层 `2*c,2*r` 周围的 3x3 或 5x5 binomial kernel 平滑下采样得到。该入口在 filters 管线中承担 organized smoothing + subsampling 职责。

## 2. 函数级候选表

| 候选 | 优先级 | 结论 | 覆盖 / 回退 |
| --- | --- | --- | --- |
| dense `Pyramid<PointXYZ>::compute` small kernel | 中 | 已接入生产 RVV | `PointXYZ`、organized、`is_dense=true`、`large_=false`、未显式请求多线程 `threads_<=1`、宽高满足生成下一层；按 VL chunk 计算输出列 |
| dense `Pyramid<PointXYZ>::compute` large kernel | 中 | 回退标量 | 初版 RVV 诊断曾出现 checksum 不一致；修正后仍不作为生产主路径，bench 用于 fallback 成本 |
| non-dense `PointXYZ` | 中 | 回退标量 | 需 `isFinite`、中心点距离阈值、动态 weight 归一化，mask/权重路径更复杂 |
| `PointXYZI` / 其它泛型 `PointT` | 中 | 回退标量 | 泛型点类型只保证 `x/y/z` 语义，不假设字段布局和额外字段处理 |
| `PointXYZRGB` / `PointXYZRGBA` / `RGB` 特化 | 中 | 回退标量 | 位于 `filters/src/pyramid.cpp`，含颜色通道 float 累加再转 `uint8_t`，先不混入本主题 |

## 3. RVV 实现计划与实际处理

实现结构：

- `Pyramid<PointT>::computeStd` 常驻标量 helper，复用原入口逻辑；函数内引入 `pcl::common` 点类型运算符以保持原标量表达式可解析；
- `__RVV10__` 下新增 `pyramidPointXYZDenseRVV` / `pyramidPointXYZDenseLevelRVV`；
- 公开 `compute` 保持 API 不变，`initCompute()` 后仅在 `PointT=PointXYZ`、dense、small kernel、未显式请求多线程、尺寸足够时短路到 RVV，否则落回 `computeStd`；
- RVV load/store 复用 `pcl/rvv_point_load.h` 与 `pcl/rvv_point_store.h` 的 indexed load / strided store 封装；
- helper 不使用 `_rm` intrinsic，不修改 FRM/FCSR。

RVV 组织：

- 一个 VL chunk 对应同一输出行的连续输出列 `c..c+VL-1`；
- 对每个 kernel tap `(m,n)`，输入列为 `2*c + (n-center)`，因此在 lane 内是 stride-2 并且边界 lane 可能 clamp 到 0 或 `width-1`；
- 为保持边界 clamp 与标量完全一致，输入采用按点 index 的 gather load；输出列连续，采用 AoS strided store 写回 `x/y/z`；
- 3x3 kernel tap 顺序与标量相同，按 `vfmacc` 累加 `x/y/z`。

## 4. 发现的问题与处理

1. 独立专项编译暴露出 `computeStd` 抽出后原模板表达式需要显式引入 `pcl::common` 点类型运算符。处理为在 `computeStd` 内添加 `using namespace pcl::common;`，并保留原来的 `next.at(j,i) += previous.at(jj,ii) * kernel_(mm,nn)`、`next.at(j,i) *= weight` 等标量表达式；同时补 `pcl::isFinite` 命名空间。这样 fallback 路径保持原 compute 语义。
2. 初版 5x5 large-kernel RVV 诊断曾出现 std/RVV checksum 不一致。处理为将生产 RVV 覆盖收窄到 small kernel，5x5 large-kernel 回退标量。后续 QEMU 与板卡 bench 中 large-kernel checksum 对齐，case 用于证明 fallback 成本接近，不作为 RVV 主路径结论。
3. QEMU 上 small-kernel RVV 明显慢于标量，但 QEMU 只用于构建、checksum、日志格式和指令路径证据；真实性能以 Milkv-Jupiter 板卡结果为准。

## 5. 测试与 bench 计划

专项文件：

- `test-rvv/filters/pyramid/test_pyramid.cpp`
- `test-rvv/filters/pyramid/bench_pyramid.cpp`
- `test-rvv/filters/pyramid/Makefile`
- `test-rvv/filters/pyramid/board.mk`

测试覆盖：

- dense `PointXYZ` small kernel，命中 RVV，并与手写 scalar reference 对比；
- dense `PointXYZ` large kernel，fallback，并与手写 scalar reference 对比；
- non-dense `PointXYZ` fallback；
- `PointXYZI` fallback；
- `initCompute` 失败路径。

上游测试：仓库 `test/filters` 没有直接针对 `pcl::filters::Pyramid` 的单测；`test_filters.cpp` 覆盖范围远大于当前入口，因此本主题不强制新增上游对拍。专项测试直接覆盖当前入口。

## 6. 验证记录

| 项目 | 结果 | 日志 |
| --- | --- | --- |
| QEMU 专项 std/RVV 测试 | 通过，6 个测试 | `test-rvv/filters/pyramid/output/qemu/run_test_std.log`、`run_test_rvv.log` |
| QEMU bench compare | 通过，checksum 对齐，分析日志可解析 | `test-rvv/filters/pyramid/output/qemu/analyze_bench_compare.log` |
| 反汇编 | 命中 `vid.v`、`vfmacc.vf`、`vsetvli` 等 RVV 指令 | `test-rvv/filters/pyramid/build/asm/riscv/bench_pyramid_rvv.full.asm` |
| 板卡专项测试 | 通过，6 个测试 | `test-rvv/filters/pyramid/output/board/run_test.log` |
| 板卡 bench compare | 通过，分析日志可解析 | `test-rvv/filters/pyramid/output/board/analyze_bench_compare.log` |

Milkv-Jupiter 板卡结论：

- `pyramid pointxyz dense 640x480 small-kernel`：Std `57.2217` ms/iter，RVV `26.4954` ms/iter，`2.16x`；
- `pyramid pointxyz dense 1280x720 small-kernel`：Std `166.8621` ms/iter，RVV `78.2088` ms/iter，`2.13x`；
- `pyramid pointxyz dense 640x480 large-kernel`：Std `116.3473` ms/iter，RVV `115.3377` ms/iter，`1.01x`，这是 fallback 语义验证，不作为主路径性能结论；
- `pyramid pointxyz non-dense fallback 640x480`：Std `86.2354` ms/iter，RVV `86.0997` ms/iter，`1.00x`；
- `pyramid pointxyzi fallback 640x480`：Std `63.6246` ms/iter，RVV `63.9966` ms/iter，`0.99x`。

## 7. 状态

`Pyramid<PointXYZ>::compute` dense small-kernel 主路径已完成生产 RVV 接入。显式多线程、large kernel、non-dense、RGB/RGBA/RGB 和非 `PointXYZ` 保持标量 fallback。主题文档：`doc-rvv/filters/pyramid-RVV.zh.md`。
