# BRISK 2D Correctness Tests

## gtest 字典

| TEST | 输入 | 被测路径 | 断言 | 证明范围 |
| --- | --- | --- | --- | --- |
| `HalfSampleLayerMatchesScalarReferenceForEvenAndTailWidths` | 32x24、34x26、641x481 合成 `uint8_t` organized image | `brisk::Layer(source, HALFSAMPLE)` 和 test-only candidate wrapper | derived image 宽高等于 `width / 2`、`height / 2`，并逐字节匹配 reference | 证明 RISC-V / 非 x86 路径不再落到空实现；覆盖常规和 tail 尺寸。 |
| `TwoThirdSampleLayerMatchesScalarReferenceForBlockAndTailWidths` | 30x24、33x27、640x480 合成图像 | `brisk::Layer(source, TWOTHIRDSAMPLE)` 和 test-only candidate wrapper | derived image 宽高等于 `2 * width / 3`、`2 * height / 3`，并逐字节匹配 reference | 证明 3x3 -> 2x2 weighted formula（加权公式）和行尾边界。 |
| `ScaleSpaceConstructPyramidProducesNonZeroDerivedImages` | 80x60 合成图像 | `ScaleSpace(2)::constructPyramid` 与派生 `HALFSAMPLE` 层 | 派生图像 checksum 非零 | 证明生产 helper 链能被 `ScaleSpace` 构造路径触达。 |
| `PublicComputeRunsOnSyntheticOrganizedCloud` | 160x120 synthetic organized `PointXYZRGBA` cloud | `BriskKeypoint2D<PointXYZRGBA>::compute()` | 输出是一维 keypoint cloud，keypoint 数非零，坐标和 scale 有限且落在输入图像范围内 | 证明公开入口在 RISC-V / 非 SSSE3 路径下能通过 downsample helper 链和标量 detector 完成。 |

## 参考链路

`include/impl/brisk_2d_downsample_reference.hpp` 定义测试专用 reference path（参考链路）。
RISC-V / 非 SSSE3 的语义按可移植标量公式冻结：

- `halfsample`：`(a + b + c + d) / 4`，整数向下取整。
- `twothirdsample`：3x3 source block 生成 2x2 output block，权重为 `4/2/1`，总除数为 `9`。

SSSE3 full-block 路径使用 `_mm_avg_epu8`，该指令有向上取整行为；因此本 topic 不声称 RISC-V 路径与
SSSE3 full-block bit-identical（逐位相同）。当前证据只冻结 RISC-V / 非 x86 portable scalar 语义。

公开入口测试的 synthetic cloud 只用于 smoke（小型验证）和 Std/RVV 对齐。它证明 `compute()` 可以运行并产生稳定
keypoint 输出，不证明真实图像 workload 的 keypoint quality（关键点质量）或 descriptor（描述子）结果。
