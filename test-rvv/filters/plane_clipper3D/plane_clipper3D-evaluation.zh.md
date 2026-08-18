# filters/plane_clipper3D 函数级 RVV 评估

## 范围

- 主文件：`filters/include/pcl/filters/impl/plane_clipper3D.hpp`
- 公开入口：`pcl::PlaneClipper3D<PointT>::clipPointCloud3D(const PointCloud<PointT>&, Indices&, const Indices&)`
- 专项目录：`test-rvv/filters/plane_clipper3D/`
- 模块依据：`doc-rvv/library-screening/filters/filters-second-pass-retained-candidate-rescreen.zh.md` 的后续执行清单第一项。

## 函数级结论

`PlaneClipper3D` 是 `Clipper3D` 的平面裁剪实现，输入为点云和齐次平面参数 `(a,b,c,d)`，输出为位于平面正侧的点下标。`clipPointCloud3D` 在 PCL 几何裁剪管线中承担线性筛选职责；标量公式是：

```text
keep(p) = a * p.x + b * p.y + c * p.z >= -d
```

本轮实现覆盖：

- XYZ-compatible 点类型：`pcl::rvv::kRVVXYZPointCompatible<PointT>` 为 true，即
  standard-layout 且有直接 `float x/y/z` 成员；
- `indices.empty()` 的全云扫描；
- 点数 `>= 64`；
- 输出 `clipped` 追加语义保持不变。

以下路径保持标量：

- 显式 subset indices：需要 gather，当前收益和维护成本不如全云路径；
- 非 XYZ-compatible 点类型：不能证明直接 `float x/y/z` member AoS load；
- 小规模输入：`vsetvl` / `vcompress` 开销不稳定；
- `clipPoint3D`、`clipLineSegment3D`、`clipPlanarPolygon3D`：不是本轮线性全云主路径。

## RVV 实现计划

- 常驻 `clipPointCloud3DStd`，保留原有标量主体和追加输出语义；
- `__RVV10__` 下新增 `clipPointCloud3DRVV`；
- `clipPointCloud3D` 公开入口用 `if constexpr (pcl::rvv::kRVVXYZPointCompatible<PointT>)` 短路调用 RVV，否则落回 Std；
- RVV 路径按 AoS stride 读取 `x/y/z`，计算 `a*x+b*y+c*z`，用 `vmfge` 形成 keep mask，用 `vcompress` 保序写出源 indices。

## 风险和处理

| 风险 | 处理 |
| --- | --- |
| 原函数对 `clipped` 是追加而不是清空 | RVV 先记录 `old_size`，只在尾部追加，再按实际 keep 数 resize |
| NaN / Inf 语义 | 标量没有 dense / finite 分支；RVV 直接用浮点比较，NaN 比较为 false，Inf 按 IEEE 比较参与 |
| 泛型 PointT 布局 | 当前实现使用公共 `kRVVXYZPointCompatible`；`PointXYZI` 会进入 RVV，QEMU 对拍已覆盖 |
| subset indices gather | 保持标量，专项 test 和 bench 覆盖 fallback |
| 浮点舍入环境 | 不使用 `_rm` intrinsic，不修改 FRM/FCSR |

## 测试计划

专项测试：`test-rvv/filters/plane_clipper3D/test_plane_clipper3D.cpp`

- 小规模点云按公式对拍；
- 大规模 `PointXYZ` 全云保序输出；
- `clipped` 追加语义；
- 显式 subset fallback；
- `PointXYZI` XYZ-compatible 路径与标量公式一致。

上游测试：`test/filters/test_clipper.cpp` 当前主要覆盖 `BoxClipper3D` 与 `CropBox`，没有直接的 `PlaneClipper3D` case；专项 Makefile 保留 `run_upstream_test_compare` 入口用于确认 clipper 相关上游测试不回归，但本主题不强制新增上游测试。

## Bench 计划

专项 bench：`test-rvv/filters/plane_clipper3D/bench_plane_clipper3D.cpp`

输出保持可解析：

- `Dataset: synthetic PointXYZ/PointXYZI clouds; full-cloud plane clipping RVV cases and subset fallback case`
- `Iterations: 30`
- 每个 case 一行 `<name> : <avg> ms/iter`，详情行包含 `Total Time` 和 checksum。

case：

| case | 入口 | 规模 / 参数 | 路径含义 |
| --- | --- | --- | --- |
| `plane_clipper3D pointxyz balanced 64K` | `clipPointCloud3D` | 64K `PointXYZ`，约平衡保留率 | RVV 主路径，小规模以上正确性与成本 |
| `plane_clipper3D pointxyz balanced 1M` | `clipPointCloud3D` | 1M `PointXYZ`，平面 dot + compress | RVV 主路径性能主 case |
| `plane_clipper3D pointxyz mostly-keep 1M` | `clipPointCloud3D` | 1M，较高保留率 | 验证 `vcompress` 在高输出量下的主路径表现 |
| `plane_clipper3D subset fallback 1M` | `clipPointCloud3D(..., subset)` | 1M 的一半 subset | fallback 语义 / 成本，不作为 RVV 性能结论 |
| `plane_clipper3D pointxyzi xyz-compatible 1M` | `PlaneClipper3D<PointXYZI>` | 1M `PointXYZI` | XYZ-compatible RVV 主路径；证明额外字段点类型 checksum 对齐 |

## 当前状态

- 函数级评估：完成。
- RVV 实现：已接入 XYZ-compatible 全云主路径；`PointXYZ` 是已量化板卡性能主 case。
- 专项 test / bench / Makefile / board.mk：已建立。
- QEMU 对拍：`make -C test-rvv/filters/plane_clipper3D run_test_compare` 通过，std/RVV 均通过 5 个专项测试。
- QEMU bench：`make -C test-rvv/filters/plane_clipper3D run_bench_compare` 通过，std/RVV checksum 对齐；`output/qemu/analyze_bench_compare.log` 无 `未解析`、`n/a`、`Total Time 不计算`。QEMU 主路径 RVV 慢于 Std，只记录为模拟器执行现象，不作为性能结论。
- 反汇编：`make -C test-rvv/filters/plane_clipper3D dump_bench_rvv` 生成 `build/asm/riscv/bench_plane_clipper3D_rvv.full.asm`，确认 `vlse32.v`、`vfmul.vf`、`vfmacc.vf`、`vmfge.vf`、`vcompress.vm`、`vcpop.m`、`vse32.v`、`vsetvli ... e32,m2`。
- 上游测试：`make -C test-rvv/filters/plane_clipper3D run_upstream_test_compare` 通过，std/RVV 两套 `test/filters/test_clipper.cpp` 均通过 `BoxClipper3D.Filters` 与 `CropBox.Filters`。该上游测试没有直接 `PlaneClipper3D` case，但可作为 clipper/crop_box 相关回归补充。
- 板卡验证：`make -C test-rvv/filters/plane_clipper3D run_board_test run_board_bench_compare fetch_board_logs` 通过，日志已拉回 `test-rvv/filters/plane_clipper3D/output/board/`。

## 板卡结果

设备：`Milkv-Jupiter`。数据集：synthetic `PointXYZ` clouds，full-cloud plane clipping RVV cases and subset/type fallback cases。Iterations: `30`。该板卡结果来自原始 `PointXYZ` closeout；本轮类型放宽的正确性和日志格式证据以 QEMU `pointxyzi xyz-compatible` case 为准。

| case | Std ms/iter | RVV ms/iter | speedup | 结论 |
| --- | ---: | ---: | ---: | --- |
| `plane_clipper3D pointxyz balanced 64K` | 1.6313 | 0.5439 | 3.00x | `PointXYZ` 全云主路径，64K 规模保序压缩收益成立 |
| `plane_clipper3D pointxyz balanced 1M` | 25.8590 | 8.9499 | 2.89x | 1M 主路径，plane dot + `vcompress` 是本主题真实性能结论核心 |
| `plane_clipper3D pointxyz mostly-keep 1M` | 32.7247 | 13.1726 | 2.48x | 高保留率下写出量增加，RVV 仍保持收益 |
| `plane_clipper3D subset fallback 1M` | 12.6639 | 12.6789 | 1.00x | 显式 subset fallback，证明未覆盖 gather 路径语义和成本接近 |
| `plane_clipper3D pointxyzi fallback 1M` | 26.4751 | 25.7216 | 1.03x | 历史类型 fallback case；不作为本轮 `PointXYZI` RVV 主路径性能结论 |

结论：`PlaneClipper3D<PointXYZ>::clipPointCloud3D` 全云主路径在板卡上约 `2.48x` 到
`3.00x`。当前源码 gate 已扩大为 XYZ-compatible；`PointXYZI` 正确性由 QEMU compare
覆盖，类型真实性能如需纳入结论，可后续单独重跑板卡。
