# Phase 020：YUV planar 生产探针

## 阶段意图和边界

本阶段进入 PI2-PI5 production integration loop（生产接入闭环）。范围只覆盖
`io/include/pcl/io/impl/lzf_image_io.hpp` 中
`LZFYUV422ImageReader::read/readOMP` 的 PCLZF planar YUV422 解压后转换：
U plane、Y plane、V plane 到 `PointT::r/g/b`。

不覆盖 `LZFDepth16ImageReader`、`LZFRGB24ImageReader`、`LZFBayer8ImageReader`、
`pcl::lzfDecompress`、文件读取、header 解析和 ImageGrabber 调度。PI5 完成后必须暂停，
保留 production diff（生产差异），由用户决定采纳或回滚。

## 当前状态清单

| area | current state |
| --- | --- |
| phase 000 diagnostic | `yuv422_planar_rgb_640x480` 板卡 5-run mean 1.1505x、median 1.1490x；Evidence Doctor 无阻塞 finding。 |
| phase 010 PI1 | 已冻结生产范围、fallback、点类型 gate 和 PI5 暂停规则。 |
| production | 尚未修改 `io/include/pcl/io/impl/lzf_image_io.hpp`。 |
| tests | 当前 gtest 只覆盖 test-only production-shaped diagnostic（生产形态诊断），尚未覆盖 production direct（真实生产路径证据）helper。 |
| board | 板卡可用；本阶段需要生产接入后重跑 repeated summary。 |

## 诊断到生产错配审计

| question | answer |
| --- | --- |
| evidence role | phase 000 为 `production-shaped diagnostic`；本阶段要升级到 `production-detail` / `production-public`。 |
| A/B boundary | phase 000 是 test helper；本阶段生产 helper 进入真实 `LZFYUV422ImageReader::read/readOMP` post-decompress 边界。 |
| 当前决策问题 | RVV-vs-scalar：真实生产边界内的 YUV planar 转换是否值得保留。 |
| diagnostic 是否可外推到 production | 只作为进入生产探针的信号。最终判断以本阶段 production direct correctness、asm、board 和 Doctor 为准。 |
| comparison-boundary / baseline mismatch 风险 | 有。诊断 POD 与 `PointT` 模板不同；生产入口还包含 cloud resize、解压后 buffer 指针转换和 `readOMP` fallback。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 当前已进入有界探针；若生产数据低于 weak-positive 或 Doctor 报 Error，PI5 暂停并建议回滚，等待用户确认。 |
| clean adoption 是否需要同一 production boundary 内的证据 | 需要；PI5 用户确认前只能写成 production candidate，不能写成 adopted。 |

## 优化矩阵

| candidate family | row source / layout | point type / layout | scope and entry | correctness / fallback target | bench / board target | asm boundary | doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `yuv422_planar_rgb_rvv` | contiguous planar U/Y/V -> AoS RGB fields | `PointT` standard-layout 且 `r/g/b` 为 `std::uint8_t`；其它可编译 RGB-like 类型 fallback | `LZFYUV422ImageReader::read/readOMP` post-decompress helper | production helper gtest；`make run_test_compare` | production case board repeated 5-run | production helper 或内联边界出现 `vlse8.v` / `vsra` / `vsse8.v` | required | planned |

## 实现和测试动作

1. 先修改 `src/test_lzf_image_io.cpp`，新增 production helper direct gtest。red 预期为编译失败，因为 production helper 尚不存在。
2. 在 `lzf_image_io.hpp` 中抽出 `convertPlanarYuv422ToPointCloudStd`，让 `read` 和 `readOMP` 的标量 fallback 共用原公式。
3. 在 `__RVV10__` 下新增 `convertPlanarYuv422ToPointCloudRVV`，只对 `r/g/b` 字段类型为 `std::uint8_t` 且 `PointT` 为 standard-layout 的类型返回 true；其它类型返回 false。
4. 更新 bench 增加 production-detail case，用真实生产 helper 做同边界性能测试。
5. 运行 QEMU correctness、RVV bench smoke、asm dump、板卡 repeated、Evidence Doctor 和 registry freshness。

## 板卡复跑预算和决策桶

本阶段使用 5-run repeated board，`iterations=20`、`warmup_iterations=3`。若 median speedup
大于等于 1.15x 且 Doctor 无 Error，判为 `positive`；`1.05x <= median < 1.15x` 判为
`weak-positive`，需要结合实现规模和 fallback 清晰度建议是否采纳；低于 1.05x 或方向摇摆则 PI5
建议不采纳或补充验证。

## 文档更新清单

完成后更新 `result.zh.md`、`optimization-matrix.zh.md`、`optimization-roadmap.zh.md`、
topic-local evaluation、testing / evidence docs 和 current Handoff。只有 PI5 证据支持且用户确认采纳后，
才创建 `doc-rvv/io/lzf_image_io-RVV.zh.md`。

## 继续 / 停止条件

本阶段默认推进到 PI5 暂停。停止条件为：生产 helper 无法安全编译、fallback gate 不能隔离、
QEMU correctness 失败、反汇编不能归属、板卡不可用或 production board 证据不成立并需要用户判断。
