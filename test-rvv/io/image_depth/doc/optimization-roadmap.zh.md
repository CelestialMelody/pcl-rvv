# image_depth optimization roadmap

## 当前边界

当前 topic 覆盖 `io/src/image_depth.cpp` 中 `DepthImage::fillDepthImage()` 和 `fillDisparityImage()` 的 depth / disparity conversion（深度 / 视差转换）主循环。phase 050 已接入 production patch 并完成 production-public（真实公开入口）证据；用户已确认采纳有收益的优化。当前 adopted production behavior 是 depth contiguous、disparity contiguous 和 disparity downsample；depth downsample 保持标量 fallback。正式长期文档为 `doc-rvv/io/image_depth-RVV.zh.md`。

## 候选搜索空间

| candidate family | idea source | applies to | expected benefit | risk / unknown | required evidence | status | next phase |
| --- | --- | --- | --- | --- | --- | --- | --- |
| contiguous depth meters RVV | 当前源码连续 `uint16_t` 到 `float` 转换 | `fillDepthImage`，`xStep == 1` | 减少逐像素分支和转换成本 | NaN 语义、padding、invalid mask | correctness、QEMU smoke、asm、board、doctor | adopted | 无 |
| contiguous disparity RVV | 当前源码 `constant / pixel` 热点 | `fillDisparityImage`，`xStep == 1` | RVV 批量除法可能优于标量循环 | 除法吞吐、invalid lane 写 0；variance warning | correctness、QEMU smoke、asm、board、doctor | adopted | 可选扩大 20-run |
| disparity stride-load downsample RVV | 队列风险项中的 downsample stride | `fillDisparityImage`，`xStep > 1` | 覆盖视差下采样入口 | `vlse16` 有 variance warning | production-public correctness、fallback、asm、board、doctor | adopted | 可选扩大 20-run |
| depth stride-load downsample RVV | 队列风险项中的 downsample stride | `fillDepthImage`，`xStep > 1` | 原希望覆盖深度下采样入口 | 接入后 production-public 不支持稳定收益 | fallback test、production-public context summary | rejected for current production patch | 不建议作为当前默认优化继续推进 |
| OpenNI legacy parity | 队列同构文件 | `openni_camera/openni_depth_image.cpp` | 复用同一公式族 | 旧 OpenNI 依赖和 wrapper API 不同 | legacy wrapper audit、correctness smoke | deferred | 另开 legacy parity topic |

## 阶段反思新增路线

| phase | new idea | why now | evidence needed | priority |
| --- | --- | --- | --- | --- |
| 000 | contiguous positive 后应先做窄范围 PI1，而不是直接尝试 downsample | depth/disparity 全尺寸和 padded case 都正向；production direct 缺口比 stride-load 更靠近当前决策 | production direct correctness、fallback、asm、board、Evidence Doctor | high |
| 000 | downsample 单独拆 phase | 本阶段 RVV build 的 downsample 是 scalar fallback，不能用单次 board 混入 contiguous 结论 | `vlse16`/gather 方案、padding/stride correctness、board repeated | medium |
| 020 | downsample `vlse16` 也形成局部生产候选 | depth / disparity downsample repeated board 均为 positive，但 Evidence Doctor 有 long-tail Warning | 若进入 production，需真实入口 gate / fallback、production asm、repeated board 扩大 runs 或补环境 metadata | high after PI2 authorization |
| 030 | PI1 应同时承载 contiguous 与 downsample 候选 | phase 020 改变了生产探针候选范围；旧 PI1 只写 contiguous 会让下一轮 PI2 错过 downsample gate | 修订后的 PI1、production direct test matrix、Evidence Doctor / registry freshness | high |
| 040 | doc-suite parity 已补齐 | board / Evidence Doctor topic 需要不依赖聊天记录的 README、测试总览、bench/evidence 和代码地图 | 路径限定 status、fresh registry、phase result | adopted |
| 050 | production-public 结果支持缩窄接入 | 真实 `DepthImage` 入口 5-run 后，contiguous 和 disparity downsample positive；depth downsample 已回退标量，context median 0.99x | 用户已确认采纳；长期 `doc-rvv` 使用 production-public 数据；若需增强信心可扩大 disparity warning case | adopted |

## 暂缓 / 拒绝路线

| candidate family | reason | resume condition |
| --- | --- | --- |
| `fillDepthImageRaw` RVV | raw path 的常见 full-size tight row 入口直接走 `memcpy`；非 memcpy 分支只复制 `uint16_t` 并处理 invalid sentinel，当前缺少 profile 证明它是主成本 | 若 float/disparity production probe 完成且 raw path profile 指向非 memcpy conversion loop |

## 默认恢复队列

| priority | action | 状态 | resume condition |
| ---: | --- | --- | --- |
| 1 | S11 production doc closeout | adopted / complete | `doc-rvv/io/image_depth-RVV.zh.md` 已创建，使用 production-public 数据。 |
| 2 | disparity warning case 20-run 扩大验证 | optional / deferred | `prod_disparity_full_640x480` 和 `prod_disparity_downsample_640x480_to_320x240` 当前均 positive，但有 long-tail warning；若用户希望更强稳定性，再扩大这两个 case。 |
| 3 | OpenNI legacy parity | deferred | 复核 legacy wrapper 差异；该动作涉及另一个 production file，不作为当前 topic 默认动作。 |
| 4 | `fillDepthImageRaw` low-priority diagnostic | profile-gated deferred | 若 profile 指向 raw conversion 或用户要求扩大到 raw path。 |
