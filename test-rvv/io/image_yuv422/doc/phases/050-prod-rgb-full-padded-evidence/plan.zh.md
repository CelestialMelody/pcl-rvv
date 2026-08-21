# Phase 050: prod-rgb-full-padded-evidence Plan

## 阶段意图和边界

本阶段补齐已采纳 `ImageYUV422::fillRGB` full-size RVV path 在 padded RGB output（带行尾 padding
的 RGB 输出）下的 production-public repeated board evidence（真实公开入口重复板卡证据）。当前 production
源码已经通过 `rgb_line_step` 支持 padding；QEMU correctness 覆盖该语义，但正式 5-run production
性能证据只覆盖 `prod_rgb_full_640x480` 和 `prod_rgb_downsample_640x480_to_320x240`。

本阶段不修改 production 源码，不扩大灰度、OpenNI legacy、其它 resize ratio 或 public API。若 padded
case 结果 positive，结论只加强 full-size adopted production behavior 的证据链；若 weak / neutral /
negative，则不自动回滚已采纳补丁，而是降级 padded 证据边界并交给后续 EvidenceDecision 讨论。

## 当前状态清单

| 项目 | 当前状态 | 路径 / 证据 |
| --- | --- | --- |
| full-size RGB | adopted | Phase 020 production public mean `2.0297x`，Doctor `Errors=0, Warnings=0, Suggestions=0`。 |
| RGB downsample | adopted | Phase 040 production public mean `3.8150x`，Doctor `Errors=0, Warnings=1, Suggestions=0`。 |
| padded correctness | pass | `run_test_compare` 的 production direct full-size padding 测试通过。 |
| padded bench label | present | `prod_rgb_full_padded_640x480` 已在 `src/bench_image_yuv422.cpp` 中存在。 |
| repeated board target | missing | 需要补 Makefile target、summary 和 Evidence Doctor 复制规则。 |

## 优化矩阵

| candidate family | row source policy | scope and entry | correctness / fallback | bench target | board evidence | asm boundary | doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| rgb-segment-store-u8 | image row contiguous with padded output | `ImageYUV422::fillRGB` full-size, even width, RGB line padding | pass `run_test_compare` | `prod_rgb_full_padded_640x480` | planned 5-run | existing production symbol asm | planned | pending |

## 实现和测试动作

1. 在 `Makefile` 中增加 `prod_rgb_full_padded_640x480_repeat_5` 的 collect、summary、manifest、Evidence Doctor target。
2. 运行 `make -C test-rvv/io/image_yuv422 run_board_yuv422_prod_rgb_full_padded_repeated`。
3. 运行或确认 Evidence Doctor 输出复制到 padded repeat 目录。
4. 更新 `result.zh.md`、optimization matrix、roadmap、evaluation 和正式 `doc-rvv` 的证据边界。

## Evidence Doctor 和 registry 规则

输入为 `log/board/prod_rgb_full_padded_640x480_repeat_5/summary.md` 和同目录 raw run logs 生成的
`evidence_manifest.json`。期望 `Errors=0`；若出现 Warning，保留 min / median / max 并说明是否影响
full-size adopted 结论。raw logs 仍按 summary-only 策略默认不提交。

## 板卡复跑预算和决策桶

本阶段预算为 5-run，每个 run 使用 `iterations=20`、`warmup=3`。决策桶：

- `positive`：mean / median 均大于 `1.10x`，min 大于 `1.00x`，Doctor 无 Error。
- `weak-positive`：mean 大于 `1.03x` 但 min 接近或低于 `1.00x`。
- `neutral`：`0.98x` 到 `1.03x`。
- `negative`：低于 `0.98x`。
- `unstable`：预算内跨桶摇摆或 Doctor Error 未闭合。

## Diagnostic 到 Production Mismatch Audit

| question | answer |
| --- | --- |
| evidence role | production-public |
| A/B boundary | public overload |
| 当前决策问题 | adopted production behavior 的 padded output stride 证据补强 |
| diagnostic 是否可外推到 production | 不使用 diagnostic 外推；本阶段直接测 production public case。 |
| comparison-boundary / baseline mismatch 风险 | 低；Std/RVV 使用同一 bench label、同一 padding 和 checksum policy。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 不新增 production probe；若 padded 退化，只降级 padded 证据边界并记录后续取舍。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 不需要；当前不是 RVV-family-selection。 |

## 继续 / 停止条件

本阶段完成后审计剩余候选：

- 灰度 RVV：已有历史负向 / 不稳定证据，若无新输入或 profile，不建议继续。
- 其它 resize ratio：当前 `fillRGB` 生产语义本身只允许偶数 downsample ratio，不能在本 phase 扩大。
- OpenNI legacy：当前文件外入口，需要另开或明确扩展授权。

若 padded evidence positive 且剩余候选均不值得继续，本 topic 可以进入 stop-for-review / ready-for-review
整理；若 padded evidence 退化或 Doctor Error，需要按 result 降级或交给用户判断。
