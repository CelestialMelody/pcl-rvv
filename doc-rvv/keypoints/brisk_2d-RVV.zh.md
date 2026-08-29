# BRISK 2D Downsample RVV

## 当前状态

`keypoints/src/brisk_2d.cpp` 已采用 RISC-V RVV（RISC-V 可变长向量）实现，覆盖 BRISK
`Layer::halfsample()`、`Layer::twothirdsample()` 和 `ScaleSpace::constructPyramid()` 中的 downsample
helper chain（下采样 helper 链）。当前生产结论是 `adopted / weak-positive`：Phase 000 板卡 repeated board
结果显示 `constructPyramid` 640x480 case median speedup `1.175x`，`twothirdsample` median `1.115x`。
Phase 010 补充了 `BriskKeypoint2D::compute()` public entry（公开入口）证据，synthetic 320x240 case
median speedup `1.017x`；同批 current repeated board 中 `constructPyramid` median `1.136x`、
`twothirdsample` median `1.108x`，说明 downsample helper 链仍为弱正向，但完整公开入口影响接近中性。

本主题不声明 AGAST/OAST detector（角点检测器）或 BRISK descriptor（描述子）路径已经加速；完整
`BriskKeypoint2D::compute()` 只在 synthetic 输入上观测到近中性影响。

## 函数语义和调用链

`BriskKeypoint2D::compute()` 在 `keypoints/include/pcl/keypoints/impl/brisk_2d.hpp` 中把 organized cloud
转换为 `image_data`，随后通过 `ScaleSpace::constructPyramid()` 构造多层 `brisk::Layer`。构造金字塔时：

- 第一派生层使用 `TWOTHIRDSAMPLE`，从 3x3 source block 生成 2x2 output block。
- 后续 octave / intra-octave 层使用 `HALFSAMPLE`，从 2x2 source block 生成 1 个 output byte。
- AGAST/OAST、score refinement 和 keypoint 输出仍保持原标量流程。

## 当前采用的优化方式

| 维度 | 当前状态 | 采用或暂缓原因 | 证据 | 边界 / 下一步 |
| --- | --- | --- | --- | --- |
| RVV `halfsample` | adopted with near-threshold note | `vlse8` 跨步读取四路 byte，扩展到 `u16` 后求和 `/4` 并写回。实现小且 fallback 清楚。 | Phase 010 current board median 1.047x / 1.039x，0/5 反向；doctor 有 near-threshold suggestion。 | 不写成强加速；后续只有 profile 指向该 helper 仍是瓶颈时再做 RVV-vs-RVV A/B。 |
| RVV `twothirdsample` | adopted | 九路 `vlse8` 读取 3x3 block group，按四个 `/9` weighted formula 生成 2x2 output。 | Phase 010 current board median 1.108x，0/5 反向，doctor 无 warning。 | 当前采用简单 stride-load 形态，不引入更复杂 packed/shuffle 实现。 |
| portable scalar fallback | adopted | 非 RVV / 非 SSSE3 平台不再报空实现。 | gtest Std / RVV 对拍通过。 | RISC-V / 非 x86 语义按 floor division 冻结。 |
| public `compute()` impact | attempted / neutral | synthetic organized `PointXYZRGBA` 输入通过完整公开入口，RVV 只加速内部 downsample helper。 | Phase 010 board median 1.017x，0/5 反向，doctor 有 near-threshold suggestion。 | 不写成完整 pipeline 显著加速；真实 workload 仍需 profile。 |
| AGAST/OAST / refinement | scalar-only | 深分支 detector 和输出状态机不属于本阶段。 | public compute 中仍保持标量。 | 需要新 detector phase 和 profile。 |

## 标量路径与 RVV 路径差异

RISC-V / 非 SSSE3 的 portable scalar reference（可移植标量参考链路）使用整数向下取整：

```text
halfsample = (a + b + c + d) / 4
twothirdsample = (4 * corner + 2 * adjacent + center) / 9
```

SSSE3 full-block 路径使用 `_mm_avg_epu8`，存在 rounded average（向上取整平均）行为。本 RVV 文档只冻结
RISC-V / 非 x86 路径与 portable scalar fallback 的一致性，不声称 SSSE3 full-block 与 RVV bit-identical。

## VL chunk 图示

`twothirdsample` 的一个 VL chunk（可变向量长度分块）按 block group 处理：

```text
source rows:
  A1 A2 A3 | A1 A2 A3 | ...
  B1 B2 B3 | B1 B2 B3 | ...
  C1 C2 C3 | C1 C2 C3 | ...

RVV:
  vlse8 stride=3 读取 A1/A2/A3/B1/B2/B3/C1/C2/C3
  u16 公式生成 upper_left, upper_right, lower_left, lower_right
  vsse8 stride=2 写入 2x2 output block
```

## Fallback 矩阵

| 条件 | 行为 |
| --- | --- |
| `__SSSE3__ && !__i386__` | 保持既有 SSSE3 路径。 |
| `__RVV10__ && __riscv_vector` 且非 SSSE3 | 使用 RVV downsample helper。 |
| 非 SSSE3 且非 RVV | 使用 portable scalar fallback。 |
| 完整 `compute()` 中 AGAST/OAST、refinement、keypoint 输出 | 保持标量。 |
| 非 contiguous image / descriptor path / features BRISK | 不属于本 helper 范围。 |

## Bench 与证据

当前性能证据来自 Milkv-Jupiter 板卡 5-run repeated board。QEMU（仿真器）只证明 correctness 和日志形状，
不作为性能证据。

| case | median speedup | min | max | B/A < 1 | 证据含义 |
| --- | ---: | ---: | ---: | ---: | --- |
| `brisk_halfsample_640x480` | 1.047x | 1.032x | 1.054x | 0/5 | halfsample 常规尺寸弱正向。 |
| `brisk_halfsample_641x481_tail` | 1.039x | 1.035x | 1.053x | 0/5 | tail 尺寸近阈值弱正向。 |
| `brisk_twothirdsample_640x480` | 1.108x | 1.099x | 1.112x | 0/5 | twothirdsample 稳定弱正向。 |
| `brisk_construct_pyramid_640x480` | 1.136x | 1.122x | 1.140x | 0/5 | downsample helper 链组合收益。 |
| `brisk_public_compute_320x240` | 1.017x | 1.014x | 1.023x | 0/5 | synthetic public compute 端到端影响接近中性。 |

证据入口：

- Topic README：`test-rvv/keypoints/brisk_2d/README.zh.md`
- Evaluation：`test-rvv/keypoints/brisk_2d/doc/brisk_2d-evaluation.zh.md`
- Summary：`test-rvv/keypoints/brisk_2d/doc/phases/000-current-state-and-downsample-diagnostic/repeated-evidence-summary.md`
- Evidence Doctor：`test-rvv/keypoints/brisk_2d/doc/phases/000-current-state-and-downsample-diagnostic/repeated-evidence-doctor.md`
- Public summary：`test-rvv/keypoints/brisk_2d/doc/phases/010-public-compute-end-to-end/repeated-evidence-summary.md`
- Public Evidence Doctor：`test-rvv/keypoints/brisk_2d/doc/phases/010-public-compute-end-to-end/repeated-evidence-doctor.md`

## Traceability Map

| 符号 / 文件 | 层级 | 作用 | 调用者 / 下游消费者 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- | --- |
| `Layer::halfsample` / `twothirdsample` | production helper | RVV downsample 和 fallback | `ScaleSpace::constructPyramid` | production boundary | `keypoints/src/brisk_2d.cpp` |
| `ScaleSpace::constructPyramid` | production helper chain | 调用 downsample 构造 pyramid | `BriskKeypoint2D::compute()` | adopted helper chain | `keypoints/src/brisk_2d.cpp` |
| `BriskKeypoint2D::compute()` | production public entry | cloud 转 image 并调用 `ScaleSpace` / detector | 用户公开 API | public-entry impact evidence | `keypoints/include/pcl/keypoints/impl/brisk_2d.hpp` |
| `run_test_compare` | correctness target | Std/RVV gtest 对拍 | reviewer / worker | correctness gate | `test-rvv/keypoints/brisk_2d/Makefile` |
| `collect_repeated_board_evidence` | board target | 5-run board test + bench | Evidence Doctor | board performance | `test-rvv/keypoints/brisk_2d/Makefile` |
| `collect_public_repeated_board_evidence` | board target | 5-run public compute board bench | Evidence Doctor | public-entry impact | `test-rvv/keypoints/brisk_2d/Makefile` |
| `generate_brisk_2d_evidence_manifest.py` | analysis script | 生成 manifest 和 summary | global Evidence Doctor | evidence manifest | `test-rvv/keypoints/brisk_2d/script/` |

## 正确性与高效性证据链

Correctness：gtest 使用真实 `brisk::Layer` 构造派生层，并与 topic-local scalar reference 逐字节对拍；
Phase 010 board repeated 每轮 unit test 都是 4/4 pass，包含 public compute smoke。

Path / asm：RVV bench 反汇编在 downsample helper 附近显示 `vlse8`、`vzext`、`vdivu`、`vse8` 和 `vsse8`。

Performance：目标硬件 repeated board 给出 downsample helper weak-positive 桶；public compute synthetic case 为
near-neutral。Phase 010 Evidence Doctor 输出 `Errors=0, Warnings=0, Suggestions=3`，suggestion 限制
halfsample 和 public compute 近阈值表述。

Boundary：结论只采纳 downsample helper 和 `ScaleSpace` helper 链；public compute 只是端到端影响审计，
不代表 AGAST/OAST detector 或 descriptor 已加速。

## Production Closeout

本次 production patch 修改 `keypoints/src/brisk_2d.cpp`，不改变公开 API。用户本轮已授权板卡有收益即可采纳；
当前 repeated board 显示 helper 链 weak-positive，因此 patch 保留为 adopted production behavior。Phase 010
公开入口 bench 显示完整 pipeline 收益接近中性；后续若继续扩大 BRISK 优化，默认从真实 workload profile 或
AGAST/OAST detector component profile 开始，而不是继续微调 downsample helper。
