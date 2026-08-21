# Phase 010：YUV planar 生产接入计划

## 阶段意图和边界

本阶段只冻结 `LZFYUV422ImageReader::read/readOMP` 的 production integration（生产接入）计划。目标是把 phase 000 中稳定正向的 `yuv422_planar_rgb_rvv` 从 test helper 迁移为有界生产候选。

本阶段不修改 production 源码；进入 PI2 前需要用户明确授权。即使后续 PI2-PI4 证据正向，PI5 仍必须暂停给用户检查生产 diff、测试命令、板卡结果和 Evidence Doctor，再由用户决定采纳或回滚。

不覆盖范围：

- `LZFDepth16ImageReader::read/readOMP`。phase 000 只有弱正向，当前暂缓。
- `LZFBayer8ImageReader::read/readOMP` 和 RGB buffer copy。phase 000 为负向。
- `LZFRGB24ImageReader::read/readOMP`。与 RGB copy 同构但未进入本轮 production probe。
- `pcl::lzfDecompress`、文件读取、header 解析和 `ImageGrabber` 调度。

## PI1 生产范围

| dimension | frozen scope |
| --- | --- |
| production entry | `LZFYUV422ImageReader::read`、`LZFYUV422ImageReader::readOMP` |
| data layout | PCLZF planar U plane + Y plane + V plane；`pixels = width * height`，`pairs = pixels / 2` |
| output | `pcl::PointCloud<PointT>` 中每个点的 `r/g/b` 字段 |
| candidate | `yuv422_planar_rgb_rvv`，每个 RVV lane 转换一个 U/V pair，并写两个点 |
| fallback | 非 RVV 构建、RGB 字段 gate 不满足、规模不合适或奇数像素数时使用原标量公式 |
| validated diagnostic point type | 测试 POD `PointXYZRGB` |
| production point type policy | 首个生产探针使用本地 `r/g/b` member gate：`PointT` standard-layout 且 `r/g/b` 成员表达式为 `std::uint8_t`；其它模板实例 fallback |

## 诊断到生产错配审计

| question | answer |
| --- | --- |
| evidence role | phase 000 是 `production-shaped diagnostic`；PI 后必须升级为 production direct。 |
| A/B boundary | phase 000 是 test helper；PI2 后应是真实 reader post-decompress helper 或 public-like wrapper。 |
| 当前决策问题 | 是否允许把 YUV planar conversion 写成有界 production probe。 |
| diagnostic 是否可外推到 production | 只可作为升级信号。PI 必须补真实 `PointT`、helper dispatch、fallback 和 production-shaped/public 证据。 |
| comparison-boundary / baseline mismatch 风险 | 有。诊断 POD 不等于生产 `PointT`；真实 reader 还有解压和 cloud resize。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 不适用于当前 YUV，phase 000 为稳定正向；后续 production direct 若低于阈值或 Doctor 报 Error，则 PI5 暂停并等待用户决定。 |
| clean adoption 是否需要同一 production boundary 证据 | 需要 production direct correctness、asm、board repeated 和 Evidence Doctor；PI5 用户确认前不能写成 adopted。 |

## 实现设计

候选生产形态：

1. 在 `io/include/pcl/io/impl/lzf_image_io.hpp` 增加局部 internal helper，保持公开 API 不变。
2. 抽出 `convertPlanarYuv422ToPointCloudStd<PointT>()`，承载当前标量公式，供 `read` / `readOMP` fallback 复用。
3. 在 `__RVV10__` 下提供 `convertPlanarYuv422ToPointCloudRVV<PointT>()`。它只在 `PointT` 满足 `r/g/b` 字段 gate、`pixels` 为偶数且输出指针可按 AoS stride 写回时返回 true；否则返回 false。
4. `read` 和 `readOMP` 在解压、cloud resize 完成后短路尝试 RVV；失败后调用 Std helper。`readOMP` 在 RVV 命中时不再并行，因为 RVV helper 自身覆盖整段转换；未命中时保持现有 OpenMP 标量 loop 语义。

生产注释只解释 planar U/Y/V 布局、模板点型 gate 和 fallback 边界，不逐行解释 intrinsic（内建函数）。

## TDD 与验证计划

PI2 前先补 production-direct failing test。建议做法：

| test / target | 预期 red | green 后证明范围 |
| --- | --- | --- |
| 新增 production helper direct gtest | 新 helper / dispatch 尚不存在，编译失败或 RVV path flag 不存在 | `PointXYZRGB` 下 production helper 与标量公式一致 |
| fallback point type gtest | fallback flag / unsupported type path 尚不可观测 | 不满足 RGB member gate 的类型不走 RVV，仍保持标量结果 |
| `read` / `readOMP` shared helper test | `read` / `readOMP` 仍各自内联循环，无法归属 helper | 两个入口共享同一 Std/RVV conversion helper |

PI2-PI4 必须运行：

- `make run_test_compare`
- 生产直连或 production-shaped `make run_bench_rvv` smoke，QEMU 仅用于日志形状
- `make dump_bench_rvv` 或 production asm target，确认 `vlse8.v`、`vsra.vi`、`vsse8.v` / 等价 store 指令归属到 YUV production helper 或内联边界
- 板卡 repeated summary，至少 5-run，`iterations=20`、`warmup_iterations=3`
- Evidence Doctor 和 evidence registry freshness

## 决策桶

生产 direct 阶段沿用 phase 000 阈值：

- `positive`：median speedup >= 1.15x 且无 Evidence Doctor Error。
- `weak-positive`：1.05x <= median speedup < 1.15x；只有实现很小、fallback 清楚、Doctor 无 Error 时可提交给用户判断。
- `neutral`：0.95x <= median speedup < 1.05x。
- `negative`：median speedup < 0.95x。
- `unstable`：5-run 内方向摇摆且一次确认复跑后仍不能稳定 bucket。

PI5 停止条件：无论证据正向还是负向，都保留当前 production diff，暂停并向用户报告是否建议采纳或回滚。

## 下一步

需要用户明确回复授权进入 PI2。授权后只对 `LZFYUV422ImageReader::read/readOMP` 做 YUV planar production probe，先补 production-direct failing test，再改 production helper，随后跑 QEMU correctness、asm、board repeated、Evidence Doctor，并在 PI5 暂停让用户决定采纳或回滚。
