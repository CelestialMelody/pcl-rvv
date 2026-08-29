# BRISK 2D 函数级评估

## 范围和目标源码

目标来自 `doc-rvv/library-screening/keypoints/keypoints-function-evaluation-queue.zh.md` 的执行清单：
`keypoints/src/brisk_2d.cpp` 中 `Layer::halfsample()`、`Layer::twothirdsample()` 和
`ScaleSpace::constructPyramid()` 的 BRISK scale-space downsample（尺度空间下采样）路径。

`keypoints/include/pcl/keypoints/impl/brisk_2d.hpp` 的公开模板入口负责把 organized input cloud（有组织点云）
转成 `image_data` 并调用 `ScaleSpace`；本轮没有修改该 header。Phase 010 已补 synthetic public-entry
evidence（公开入口证据），但只显示端到端 near-neutral（接近中性）影响。

## 函数级结论

当前结论是 `production-adopted / weak-positive`。RISC-V `__RVV10__` 构建下，downsample helper 使用 RVV
stride load（跨步加载）和整数加权；非 RVV / 非 SSSE3 构建使用 portable scalar fallback。板卡 repeated
evidence（重复证据）显示 Phase 000 `ScaleSpace::constructPyramid` 组合 case median `1.175x`，因此按本轮用户授权采纳。
Phase 010 的 `BriskKeypoint2D::compute()` public case median `1.017x`，0/5 反向，说明完整公开入口可运行且
略正向但收益被 AGAST/OAST detector 等标量阶段稀释；不把完整 BRISK pipeline 写成显著加速。

## 函数族作用速览

| 函数 / 函数族 | 作用 | 输入 / 输出状态 | 与主流程关系 | RVV 判断 |
| --- | --- | --- | --- | --- |
| `BriskKeypoint2D::compute()` | 公开 keypoint 入口 | organized cloud -> `image_data` -> `ScaleSpace` -> keypoints | 完整 BRISK pipeline | Phase 010 attempted；correctness 通过，board median 1.017x near-neutral。 |
| `ScaleSpace::constructPyramid()` | 构造 BRISK 金字塔 | 原图和 octave 数 -> 多层 `Layer` | downsample helper 链的生产调用者 | adopted 间接受益。 |
| `Layer::halfsample()` | 2x 缩小图像 | 2x2 `uint8_t` block -> 1 byte | pyramid 内热点 helper | adopted，收益近阈值。 |
| `Layer::twothirdsample()` | 2/3 缩小图像 | 3x3 block -> 2x2 bytes | pyramid 第一派生层 | adopted，稳定弱正向。 |
| AGAST/OAST / refinement | 角点检测和尺度细化 | scores / keypoints | 下游主流程 | 未覆盖，另行评估。 |

## 标量流程与 RVV 流程对照

| 阶段 | 标量 / 旧行为 | RVV / 新行为 | 边界 |
| --- | --- | --- | --- |
| 非 SSSE3 fallback | 旧代码报 `PCL_ERROR`，RISC-V 路径不可用 | portable scalar fallback 生成派生图像 | 修复非 x86 可运行性。 |
| `halfsample` | 2x2 byte 求和后 `/4` | `vlse8` 读取四路 byte，`u16` 求和，`vdivu /4`，`vse8` 写回 | RISC-V 语义按 floor division。 |
| `twothirdsample` | 3x3 block 加权生成 2x2 | `vlse8` 读取九路 byte，四个公式向量化，`vsse8` 跨步写回 | 只覆盖 contiguous image。 |
| AGAST / keypoint output | 标量深分支和动态输出 | 保持标量 | 不改变输出顺序和公开 API。 |

## 实现方式审计

| 实现维度 | 当前状态 | 证据 | 边界 / 恢复条件 |
| --- | --- | --- | --- |
| dispatch / fallback | adopted | `__RVV10__ && __riscv_vector` 下走 RVV，否则 portable scalar / SSSE3 | 不新增公开 API。 |
| layout | adopted | 测试和 bench 使用 contiguous `uint8_t` image | 不涉及 PCL point type traits。 |
| formula | adopted | gtest 逐字节对拍 reference | 不声称 SSSE3 rounded-average full-block bit-identical。 |
| asm attribution | adopted | `dump_bench_rvv`，helper 附近有 `vlse8` / `vsse8` 等 | 完整 asm dump 默认不提交。 |
| board performance | adopted weak-positive | repeated summary / doctor / registry | halfsample 单项近阈值，文档降级说明。 |
| full public compute | attempted / neutral | `PublicComputeRunsOnSyntheticOrganizedCloud`、`brisk_public_compute_320x240` | synthetic public case 正确性通过，端到端收益 near-neutral；真实 workload profile 仍未覆盖。 |

## Traceability Map

| 符号 / 文件 | 层级 | 作用 | 调用者 / 下游消费者 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- | --- |
| `Layer::halfsample` / `twothirdsample` | production helper | RVV downsample 和 scalar fallback | `ScaleSpace::constructPyramid` | production boundary | `keypoints/src/brisk_2d.cpp` |
| `Brisk2DDownsample.*` | correctness test | 逐字节对拍 | `run_test_compare` / board `run_test` | correctness gate | `test-rvv/keypoints/brisk_2d/src/test_brisk_2d.cpp` |
| `bench_brisk_2d.cpp` | bench wrapper | 输出五个 case，包含 public compute case | board repeated target | production direct performance / public-entry impact evidence | `test-rvv/keypoints/brisk_2d/src/bench_brisk_2d.cpp` |
| `generate_brisk_2d_evidence_manifest.py` | analysis script | 生成 manifest / summary | Evidence Doctor | evidence manifest | `test-rvv/keypoints/brisk_2d/script/` |
| repeated summary / doctor | output summary | 保存 5-run 统计和异常检查 | evaluation / doc-rvv | board evidence | `test-rvv/keypoints/brisk_2d/doc/phases/000-current-state-and-downsample-diagnostic/` |
| Phase 010 repeated summary / doctor | output summary | 保存 public compute 5-run 统计和异常检查 | evaluation / doc-rvv | public-entry impact evidence | `test-rvv/keypoints/brisk_2d/doc/phases/010-public-compute-end-to-end/` |

## 生产接入后的最终证据更新

| 项 | 当前证据 |
| --- | --- |
| `production_patch_scope` | `keypoints/src/brisk_2d.cpp` 新增 RVV include、portable scalar fallback、RVV halfsample/twothirdsample。 |
| `covered_path` | `brisk::Layer` downsample helper 和 `ScaleSpace::constructPyramid` helper chain。 |
| `fallback_matrix` | 非 RVV / 非 SSSE3 构建走 portable scalar；SSSE3 既有路径保持；AGAST/OAST 和 keypoint refinement 保持标量。 |
| `production_direct_tests` | `run_test_compare`、Phase 010 5-run board `run_test` 每轮 4/4 pass。 |
| `production_asm` | `bench_brisk_2d_rvv.full.asm` 中 helper 附近有目标 RVV 指令。 |
| `production_board_bench` | repeated board summary：overall decision bucket weak-positive。 |
| `public_entry_board_bench` | Phase 010 repeated board：`brisk_public_compute_320x240` median 1.017x，0/5 反向，decision bucket neutral。 |
| `decision_delta` | 早期 helper smoke 被 5-run production direct 证据确认；公开入口证据补充了端到端 near-neutral 边界。最终以 Phase 000/010 repeated summary 为当前 truth。 |

## 正确性与高效性证据链

QEMU 和 board correctness 均证明派生图像逐字节匹配 RISC-V portable scalar reference。反汇编证明 RVV
helper 命中。板卡 repeated summary 证明目标硬件上 downsample helper 链有弱正向收益；Phase 010 public compute
summary 证明完整公开入口 synthetic case 正确性闭合但收益接近中性。Evidence Doctor 没有 Error / Warning；
near-threshold suggestion 限制 halfsample 和 public compute 表述，不阻塞本轮授权下采纳 downsample helper。

## 未覆盖范围

真实 workload 下的完整 `BriskKeypoint2D::compute()` 尚未闭合。Phase 010 只使用 synthetic organized
`PointXYZRGBA` cloud；它证明公开入口能运行并给出 near-neutral 性能，但不能代表真实图像分布、descriptor
或 keypoint quality。若后续继续 BRISK 主题，应先做真实 workload profile 或 AGAST/OAST detector oracle，
而不是继续微调 downsample helper。
