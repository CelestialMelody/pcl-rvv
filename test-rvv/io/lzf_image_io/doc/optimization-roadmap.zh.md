# lzf_image_io 优化路线图

## 默认恢复动作

| phase | 范围 | 状态 | resume condition | next action |
| --- | --- | --- | --- | --- |
| 000-current-state-and-diagnostic-scaffold | post-decompress depth/YUV/Bayer-copy 诊断脚手架 | completed | phase result 已写入 | YUV planar 进入 PI1；depth 暂缓；RGB copy 拒绝 |
| 010-production-integration-plan | 只冻结 `LZFYUV422ImageReader::read/readOMP` 的 YUV planar production probe | completed | 用户已授权进入 PI2 | 已推进到 phase 020 |
| 020-yuv-planar-production-probe | 接入 YUV planar production helper 并重跑生产证据 | completed / adopted_after_user_confirmation | production-detail board 5-run mean 1.0864x、median 1.0832x，Doctor 0/0/0 | S11 production closeout 已创建 `doc-rvv/io/lzf_image_io-RVV.zh.md` |

## 候选搜索空间

| candidate family | idea source | applies to | expected benefit | risk / unknown | required evidence | status | next phase |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `depth_xyz_rvv` | 当前源码和 `image_depth` 结构经验 | depth16 plane 到 `PointXYZ` xyz | 连续 depth load、mask、公式可向量化 | AoS 写回和 `is_dense` 语义；真实 file read 成本稀释 | correctness、asm、board repeated、doctor | deferred-diagnostic | future-depth-phase |
| `yuv422_planar_rgb_rvv` | 当前源码和 `image_yuv422` 公式经验 | planar U/Y/V 到 RGB fields | 整数公式可复用 RVV widening / clip 形态 | production 收益低于诊断阶段，属于 weak-positive；用户确认有收益即可采纳 | production correctness、asm、board repeated、doctor 已完成；正式 doc-rvv 已创建 | adopted | none |
| `rgb_buffer_to_cloud_rvv` | 当前源码和 Bayer/RGB24 同构拷贝 | RGB buffer 到 cloud RGB fields | 字段写回可减少 scalar loop 开销 | Bayer 主成本可能在 debayer；收益可能很弱 | correctness、asm、board repeated、doctor | rejected-diagnostic-candidate | none |
| `full_bayer_edge_aware_rvv` | `debayer` sibling negative / deferred 经验 | Bayer raw 到 RGB | 理论上覆盖更多成本 | edge-aware stencil 尚无正向证据，复杂度高 | 另开 debayer-family phase 或 topic | deferred | separate follow-up |

## 阶段反思新增路线

| phase | new idea | why now | evidence needed | priority |
| --- | --- | --- | --- | --- |
| 000 | 只推进 YUV planar 到 PI1 | YUV 在 Milkv-Jupiter 5-run 中 median 1.1490x，Doctor 无阻塞 finding；其它两条不满足生产接入条件 | production direct correctness、fallback gate、asm、board repeated、Doctor；PI5 用户确认 | high |
| 020 | YUV production helper 收益降为 weak-positive | production-detail case 使用真实 `pcl::PointXYZRGB` 和 production helper，mean 1.0864x / median 1.0832x，仍稳定正向但没有达到 positive 桶 | 用户已确认有收益即可采纳；S11 已补正式 `doc-rvv`。更强 reader public bench 可作为信心增强，不是当前采纳前置条件。 | closed |

## 暂缓 / 拒绝路线

| candidate family | reason | resume condition |
| --- | --- | --- |
| `lzf_decompress_rvv` | LZF 解压是串行状态机，队列表明确不覆盖。 | 只有另有 profile 证明解压后转换不是主成本且用户另开 topic。 |
| `full_bayer_edge_aware_rvv` | 当前 topic 首阶段先诊断 Bayer 后 RGB copy；完整 edge-aware debayer 已属于 debayer family。 | debayer topic 或 profile 给出新的正向候选。 |
| `rgb_buffer_to_cloud_rvv` | Phase 000 板卡 median 0.9894x，5/5 低于 1；Evidence Doctor 报 `ba_degradation_frequency` Error。 | 新的写回策略、不同点型或 profile 证明 RGB copy 是真实瓶颈后另开 phase。 |
| `depth_xyz_rvv` | Phase 000 仅 weak-positive，median 1.0417x，并触发 near-threshold suggestion；invalid depth 仍需标量 lane 修正。 | 更强 code shape、扩大 runs 后稳定正向，或真实 production profile 指向 depth conversion 主导。 |
