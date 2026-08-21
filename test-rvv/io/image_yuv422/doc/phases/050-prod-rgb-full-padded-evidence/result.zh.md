# Phase 050: prod-rgb-full-padded-evidence Result

## 当前结论

Phase 050 补齐了已采纳 `ImageYUV422::fillRGB` full-size RVV path 在 padded RGB output
（带行尾 padding 的 RGB 输出）下的 production-public repeated board evidence（真实公开入口重复板卡证据）。
本阶段没有修改 production 源码，只新增 Makefile target 并复用既有 bench label。

EvidenceDecision：`adopted_evidence_strengthened`。`prod_rgb_full_padded_640x480` 5-run mean
`2.1030x`、median `2.1012x`、min `2.0889x`、max `2.1169x`；mean Std `3.2876 ms`，
mean RVV `1.5633 ms`。Evidence Doctor 为 `Errors=0, Warnings=0, Suggestions=0`。

## 动作回填

| 动作 | 状态 | 证据 | 结论 |
| --- | --- | --- | --- |
| phase plan | done | `050-prod-rgb-full-padded-evidence/plan.zh.md` | 范围限定为 full-size padded output production evidence。 |
| Makefile target | done | `run_board_yuv422_prod_rgb_full_padded_repeated` | 新增 collect、summary、manifest 和 Doctor target；不改 production。 |
| board repeated | done | `log/board/prod_rgb_full_padded_640x480_repeat_5/summary.md` | 5-run 全部 positive，min `2.0889x`。 |
| Evidence Doctor | done | `log/board/prod_rgb_full_padded_640x480_repeat_5/evidence_doctor.md` | `Errors=0, Warnings=0, Suggestions=0`。 |

## Evidence Doctor 解释

Doctor 没有 findings（发现项）。padded case 的 checksum 在 Std/RVV 间一致，`run_count=5`、
`iterations=20`、`warmup_iterations=3`，Evidence role（证据角色）为 `production_public`，
A/B boundary（A/B 边界）为 `public_overload`。本结果支持 full-size adopted production behavior
在 `rgb_line_step > width * 3` 的输出布局下仍保持强正向收益。

## 剩余候选审计

| 候选 | 当前判断 | 理由 | 后续动作 |
| --- | --- | --- | --- |
| 灰度 RVV | 不建议继续当前 topic 内推进 | 灰度只复制 Y 分量，历史诊断已显示收益不足或负向；当前 production RGB 两条路径已是主要热点。重新尝试需要新的 profile 或不同实现假设。 | 保持标量 fallback。 |
| 其它 RGB downsample ratio / resize 形态 | 当前不扩大 | `fillRGB` 公开语义只允许水平 / 垂直偶数 ratio；当前 RVV gate 与 production 语义一致。其它形态不是未实现优化点。 | not_applicable。 |
| full-size / downsample 进一步 ILP、LMUL 或 unroll | 暂不建议继续 | 当前 production public 已有约 `2.0x` 到 `3.8x` 收益，剩余可能收益需要 RVV-vs-RVV family A/B、寄存器压力和更多板卡消融；相对维护成本较高。 | 仅在后续 profile 指向 `ImageYUV422::fillRGB` 仍是瓶颈时另开 tuning phase。 |
| OpenNI legacy YUV422 | 当前 topic 外 | 需要修改 `io/src/openni_camera/openni_image_yuv_422.cpp`，不是本 topic 的 `io/src/image_yuv422.cpp` 范围。 | 另开 topic 或获得明确扩展授权。 |

## Continue / Stop Decision

当前 `io/src/image_yuv422.cpp` topic 内的高价值生产优化已闭合：full-size、full-size padded 和 RGB downsample
均有 production-public correctness、asm、5-run board 和 Evidence Doctor 证据。灰度路径保持标量更稳，
其它 resize ratio 不适用，进一步 ILP / LMUL tuning 暂无足够 profile 支撑，OpenNI legacy 属于当前文件外入口。

本阶段结论为 `stop_for_review_no_more_current_file_optimization`。默认下一步是整理 Handoff / ready-for-review；
若用户希望继续，应优先另开 OpenNI legacy 或 image_yuv422 tuning follow-up，而不是在当前 topic 内继续扩大生产补丁。
