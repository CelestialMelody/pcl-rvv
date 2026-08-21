# Phase 010 结果：YUV planar 生产接入计划

## 阶段结论

PI1 已完成。`yuv422_planar_rgb_rvv` 可以作为有界 production probe（生产探针）进入 PI2，但当前不能直接修改 `io/include/pcl/io/impl/lzf_image_io.hpp`，因为 production integration loop（生产接入闭环）的生产源码改动需要用户明确授权。

本阶段未修改 production 源码，未新增 production direct test，也未刷新板卡 repeated summary。phase 000 的诊断证据仍是当前性能依据。

## PI1 冻结范围

| item | frozen decision |
| --- | --- |
| production entry | 只覆盖 `LZFYUV422ImageReader::read` 和 `LZFYUV422ImageReader::readOMP`。 |
| data layout | 只覆盖 PCLZF planar U plane + Y plane + V plane；不覆盖 `ImageYUV422` 的 interleaved YUYV。 |
| output fields | 只写 `PointT::r/g/b`。 |
| fallback | 非 RVV build、RGB field gate 不满足、奇数像素或其它未证明布局均回到标量公式。 |
| not covered | depth、Bayer、RGB24、`pcl::lzfDecompress`、文件读取、header 解析、ImageGrabber 调度。 |
| PI5 rule | 无论 production direct 证据正向或负向，PI5 都暂停给用户检查并等待采纳 / 回滚确认。 |

## 生产实现设计审计

| area | decision | rationale |
| --- | --- | --- |
| helper shape | `convertPlanarYuv422ToPointCloudStd<PointT>()` + `convertPlanarYuv422ToPointCloudRVV<PointT>()` | 保持公开 API 不变，并把原标量公式从 `read` / `readOMP` 内联循环中抽出，便于 fallback 和 production direct 测试。 |
| RVV store strategy | 每个 lane 转换一个 U/V pair，使用 strided byte store 写两个点的 `r/g/b` | 与 phase 000 诊断 candidate 的正向路径一致；AoS point stride 在 production helper 中取 `sizeof(PointT)`。 |
| point type policy | phase-local `r/g/b` member gate | 当前公共 RVV point traits 只覆盖 xyz / normal；RGB 字段没有现成通用 gate，因此首个生产探针使用本地、可回退的 member gate。 |
| `readOMP` policy | RVV 命中时直接使用 RVV helper；未命中时保持现有 OpenMP 标量 fallback | 避免把 OpenMP 分块和 RVV 分块混合成新边界。 |
| comments | concise boundary only | production 注释只解释 planar 布局、字段 gate 和 fallback。 |

## TDD / PI2 计划状态

PI2 的第一步必须是 production-direct failing test（先失败测试）。本阶段只写计划，不留下故意失败的测试文件，避免当前 topic 处于不可通过状态。

PI2 建议最小 red test：

| red test | expected fail before production patch | purpose |
| --- | --- | --- |
| `ProductionYuv422HelperMatchesScalarForPointXYZRGB` | 编译失败或找不到 production helper / path flag | 证明新 production helper 对 `PointXYZRGB` 与当前标量公式一致。 |
| `ProductionYuv422FallbackForUnsupportedRgbFields` | 找不到 fallback observability | 证明不满足 `r/g/b` gate 的点型不走 RVV。 |
| `ProductionYuv422ReadAndReadOMPShareConversion` | 无法归属 shared helper | 证明 `read` / `readOMP` 使用同一 conversion boundary。 |

## Evidence Carryover

PI2 前可复用的 phase 000 evidence：

- correctness：`make run_test_compare`，Std/RVV 各 3 个 gtest 通过。
- asm：`build/asm/riscv/bench_lzf_image_io_rvv.asm` 中 YUV path 可见 `vlse8.v`、`vsra.vi`、`vsse8.v`。
- board summary：`log/board/production_shaped_repeat_5/summary.md`，YUV mean 1.1505x、median 1.1490x。
- Evidence Doctor：`log/board/production_shaped_repeat_5/evidence_doctor.md`，YUV 无阻塞 finding；整体 Error 指向 RGB copy。
- registry：`log/evidence_registry.json`，当前 freshness check 通过。

这些 evidence 只能支持进入 PI2，不能替代 PI2-PI4 的 production direct evidence。

## Continue / Stop Decision

当前命中合法停止条件：PI2 需要修改 production 源码，且 AGENTS.md 明确要求 production source modification（生产源码修改）不能从短 prompt 自动推导。用户若确认进入 PI2，下一轮按以下授权范围继续：

```text
只对 LZFYUV422ImageReader::read/readOMP 做 YUV planar production probe，跑到 PI5 检查点暂停。
```

未授权前，当前 topic 的测试资产、诊断文档、doc suite、队列表状态和 handoff 已可恢复；不应为了“持续推进”自行改 production 源码。
