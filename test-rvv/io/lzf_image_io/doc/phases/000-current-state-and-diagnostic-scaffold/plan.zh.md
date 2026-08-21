# Phase 000：当前状态与诊断脚手架

## 阶段意图和边界

本阶段为 `io/include/pcl/io/impl/lzf_image_io.hpp` 建立首轮 RVV 诊断资产。目标只覆盖 PCLZF 解压后的转换循环：

- `LZFDepth16ImageReader::read/readOMP` 中 depth16 buffer 到 `PointT::x/y/z` 的转换。
- `LZFYUV422ImageReader::read/readOMP` 中 planar U/Y/V buffer 到 `PointT::r/g/b` 的转换。
- `LZFBayer8ImageReader::read/readOMP` 中 `DeBayer::debayerEdgeAware` 之后 RGB buffer 到 `PointT::r/g/b` 的拷贝。

本阶段不修改 production（生产源码），不覆盖 `pcl::lzfDecompress`，不把 test-only candidate（仅测试使用候选）写成 production direct（真实生产路径证据）。`LZFRGB24ImageReader` 的 planar RGB copy 与 Bayer 后拷贝同构，本阶段只作为可复用候选记录，不作为队列表主范围。

## 当前状态清单

| 对象 | 当前事实 | 路径 |
| --- | --- | --- |
| 队列表 | 顺序 6，状态为“未启动”；要求仅覆盖 post-decompress conversion。 | `doc-rvv/library-screening/io/io-function-evaluation-queue.zh.md` |
| production 源码 | depth / RGB24 / YUV422 / Bayer reader 都在模板 header 内完成 cloud resize 和逐点写回。 | `io/include/pcl/io/impl/lzf_image_io.hpp` |
| 上游测试 | `test/io/test_grabbers.cpp` 有 PCLZF image grabber smoke（小型入口验证），但没有专项 post-decompress oracle。 | `test/io/test_grabbers.cpp` |
| topic 资产 | 本 topic 尚无 `test-rvv/io/lzf_image_io` 目录。 | `test-rvv/io/lzf_image_io` |
| 相邻经验 | `image_depth`、`image_yuv422`、`debayer` 已形成 io 模块测试/bench Makefile 和 board target 结构；只迁移结构和证据边界，不复制结论。 | `test-rvv/io/image_depth`、`test-rvv/io/image_yuv422`、`test-rvv/io/debayer` |

## 假设与候选族

| candidate family | 证据角色 | 预期收益 | 主要风险 | 首轮状态 |
| --- | --- | --- | --- | --- |
| `depth_xyz_rvv` | production-shaped diagnostic（生产形态诊断） | 连续 depth16 load、mask 和 xyz formula 可按 VL chunk（可变向量长度分块）执行。 | `PointT` 为 AoS（结构数组）写回，invalid depth 需要 NaN 和 `is_dense=false` 语义。 | planned |
| `yuv422_planar_rgb_rvv` | production-shaped diagnostic | planar U/Y/V 与 `image_yuv422` 整数公式相近，适合复用同类 RVV 公式。 | PCLZF 布局是 U plane + Y plane + V plane，不是 YUYV interleaved；不能直接外推 `image_yuv422` 的 production 结论。 | planned |
| `rgb_buffer_to_cloud_rvv` | production-shaped diagnostic | RGB24/Bayer 后 RGB buffer 到 cloud 的连续拷贝可用 segmented store（分段存储）或字段写回诊断。 | Bayer 的主要成本可能在 `DeBayer::debayerEdgeAware`，单独拷贝收益可能被稀释。 | planned |

## 诊断到生产错配审计

| question | answer |
| --- | --- |
| evidence role | 首轮为 `production-shaped diagnostic`，因为测试资产直接喂解压后 buffer，不走真实文件读取、header 解析和 `decompress`。 |
| A/B boundary | `test helper`，Std build 运行标量 reference，RVV build 运行 test-only candidate。 |
| 当前决策问题 | `RVV-vs-scalar` 和 `implementation-shape`：判断 post-decompress helper 是否值得进入 production integration loop（生产接入闭环）。 |
| diagnostic 是否可外推到 production | 暂定 `unknown`。它能说明转换循环本身，但不能证明真实 PCLZF file read、decompress、resize 和 metadata 成本后的总收益。 |
| comparison-boundary / baseline mismatch 风险 | 有。YUV422 相邻 topic 的 production patch 使用 interleaved YUYV，而本 topic 是 planar U/Y/V；Bayer 还包含 debayer 主体成本。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 允许条件是：实现范围很小、fallback 明确、且至少一个转换族在板卡 repeated summary（重复板卡摘要）中稳定正向。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 如果后续 production public positive 要用于采纳，需要 production direct test、asm attribution（反汇编归属）和 board evidence（板卡证据）；若选择新 family 替换已有 family，还需要同边界 detail A/B。 |

## 优化矩阵

| candidate family | row source / layout | point type / Scalar | correctness / fallback target | bench / board target | asm boundary | Evidence Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `depth_xyz_rvv` | contiguous depth16 plane -> AoS `PointXYZ` | `PointXYZ` / float fields | `run_test_compare` | `run_board_lzf_depth_xyz`、repeated summary | `dump_bench_rvv` 中 depth helper RVV 指令 | required before EvidenceDecision | planned |
| `yuv422_planar_rgb_rvv` | planar U/Y/V -> AoS RGB fields | `PointXYZRGB` byte fields | `run_test_compare` | `run_board_lzf_yuv422_rgb`、repeated summary | `dump_bench_rvv` 中 YUV helper RVV 指令 | required before EvidenceDecision | planned |
| `rgb_buffer_to_cloud_rvv` | RGBRGB buffer -> AoS RGB fields | `PointXYZRGB` byte fields | `run_test_compare` | `run_board_lzf_rgb_copy`、repeated summary | `dump_bench_rvv` 中 RGB copy RVV 指令 | required before EvidenceDecision | planned |

## 实现和测试动作

| action | 产物 | 完成判据 |
| --- | --- | --- |
| 1. 建立 topic scaffold | `Makefile`、`board.mk`、`include/lzf_image_io.h`、`src/test_lzf_image_io.cpp`、`src/bench_lzf_image_io.cpp` | 先写 failing correctness test（预期因 candidate 未实现或差异失败），再实现 candidate。 |
| 2. 写 depth / YUV / RGB copy 标量 reference | `include/lzf_image_io.h` | reference 复刻当前源码公式、NaN 和 RGB 字节语义。 |
| 3. 写 RVV candidate | `include/lzf_image_io.h` | `__RVV10__` 下使用 RVV intrinsic；非 RVV 构建回到 reference。 |
| 4. 运行本地验证 | QEMU `run_test_compare`、`run_bench_rvv`、`dump_bench_rvv` | QEMU correctness 通过；QEMU bench 只作为 log-shape smoke；反汇编能归属核心 RVV 指令。 |
| 5. 运行板卡验证 | board smoke、单 case bench、bounded repeated summary、Evidence Doctor | 板卡可用时执行；性能结论只来自 board summary。 |

## 板卡复跑预算和决策桶

首轮每个 candidate family 使用 5-run repeated summary。decision bucket（决策桶）暂定：

- `positive`：median speedup >= 1.15x 且无 Evidence Doctor Error。
- `weak-positive`：1.05x <= median speedup < 1.15x。
- `neutral`：0.95x <= median speedup < 1.05x。
- `negative`：median speedup < 0.95x。
- `unstable`：5-run 内方向摇摆且无法用一次补跑稳定 bucket。

用户已说明板卡可用，因此本阶段需要性能结论时必须继续跑板卡 target；只有 ssh/rsync/tool 失败、Evidence Doctor Error、dirty isolation 不安全或继续会扩大到 production 才能停。

## 文档和恢复动作

- 创建 `doc/lzf_image_io-evaluation.zh.md`，承载函数级评估、Traceability Map（可追踪性地图）和诊断证据链。
- 创建 `doc/optimization-roadmap.zh.md` 与 `doc/phases/optimization-matrix.zh.md`。
- 阶段结束写 `result.zh.md`，回填动作状态、证据路径、Evidence Doctor 和下一 phase。
- 不创建 `doc-rvv/io/lzf_image_io-RVV.zh.md`，除非后续 PI5 生产证据闭环通过且用户确认采纳。

## 继续 / 停止条件

默认继续到本阶段 correctness、QEMU log-shape、asm 和 board repeated evidence 闭合。合法停止条件只有：生产接入需要用户授权、板卡/工具真实不可用、Evidence Doctor Error 未处理、dirty isolation 不安全，或当前 roadmap / matrix 没有授权且未阻塞的下一动作。
