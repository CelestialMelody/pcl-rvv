# image_yuv422 Optimization Roadmap

## 当前边界

当前 topic 来自 `doc-rvv/library-screening/io/io-function-evaluation-queue.zh.md` 的 YUV422 color conversion。Phase 020 已采纳 `ImageYUV422::fillRGB` full-size even-width RGB RVV production patch；Phase 040 已采纳 RGB downsample 偶数 ratio gate。灰度、其它 resize ratio 和 OpenNI legacy 仍是独立未覆盖范围。

## 候选搜索空间

| candidate family | idea source | applies to | expected benefit | risk / unknown | required evidence | status | next phase |
| --- | --- | --- | --- | --- | --- | --- | --- |
| rgb-segment-store-u8 | Phase 010 编译探针 + Phase 020 production direct | 全尺寸 `fillRGB` | 2 次 `vssseg3e8` 替代 6 次 `vsse8`，降低 interleaved RGB 写回压力 | 只覆盖 full-size even-width RGB；灰度和 OpenNI legacy 保持标量或暂缓 | production direct correctness、fallback、asm、5-run board、doctor | adopted | S11 closeout |
| contiguous-rgb-strided-u8 | 当前源码公式和 RVV `vlse8` / `vsse8` 支持 | 全尺寸 `fillRGB` | 主循环整数乘加和两像素写回可并行 | interleaved RGB 写回开销偏高 | Phase 000 correctness、asm、board smoke | superseded | none |
| grayscale-strided-y-copy | 当前源码中灰度只复制 Y 分量 | 全尺寸 `fillGrayscale` | 简单跨步 load/store，风险低 | 纯内存搬运收益弱，active candidate 已收回 | scalar fallback correctness | rejected | none |
| downsample-rgb-stride-vsseg3-u8 | Phase 030 diagnostic + Phase 040 production direct | RGB downsample 640x480 -> 320x240 | `vlse8` stride sampling + segmented RGB 写回，减少标量 per-pixel 循环成本 | production 5-run 有 long-tail warning；只覆盖水平/垂直偶数 ratio gate | production direct correctness、asm、5-run board、doctor | adopted | S11 closeout |
| rgb-full-padded-evidence | Phase 050 production public evidence | full-size padded RGB output | 补齐 `rgb_line_step > width * 3` 的 production-public 证据 | 只补证据，不是新 production code shape | production public 5-run + Doctor | adopted_evidence_strengthened | stop-for-review |
| production-integration | Phase 020 production public 证据 | `ImageYUV422::fillRGB` full-size | production public 5-run mean `2.0297x` | OpenNI legacy parity 未覆盖 | PI1-PI5 production direct | adopted | S11 closeout |

## 默认恢复队列

| order | action | status | resume condition |
| --- | --- | --- | --- |
| 1 | S11 production closeout：同步 adopted 状态、提交边界和最终 Handoff。 | done | 用户已确认采纳当前 production patch；正式 `doc-rvv` 使用接入后的板卡数据。 |
| 2 | Phase 050：补 `prod_rgb_full_padded_640x480` production-public repeated board evidence。 | done | 5-run mean `2.1030x`，Doctor `Errors=0, Warnings=0, Suggestions=0`。 |
| 3 | 当前文件内剩余候选审计：灰度 RVV、其它 resize ratio、full-size / downsample 进一步 ILP 或 store 形态。 | stop_for_review_no_more_current_file_optimization | 灰度已拒绝；其它 ratio 不适用；进一步 tuning 需要新 profile；OpenNI legacy 是当前文件外入口。 |
| 4 | OpenNI legacy parity audit。 | deferred | 会触碰 `io/src/openni_camera/openni_image_yuv_422.cpp`，属于当前文件外入口；若继续应另开或获得明确扩展授权。 |
