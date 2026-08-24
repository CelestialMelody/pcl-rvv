# Phase 059 计划：custom layout alignment sensitivity

## 阶段意图和边界

本阶段继续 Phase 055-057 的 custom layout（自定义布局）方向，但只补一个更窄的 alignment sensitivity（对齐敏感性）采样：新增测试本地注册的 `alignas` xyz AoS 点型，验证当前已采纳 production public row-source path（生产公开 row-source 路径）在异常对齐和较大 stride 下的正确性、QEMU smoke（QEMU 小型路径验证）、board repeated（板卡重复性能测试）和 Evidence Doctor（证据体检）。

本阶段不修改 production 源码，不扩大 production gate（生产门控），不验证全部自定义点型全集、packed unaligned float、不合法 index / correspondence，也不进入 `Scalar=double`。

## 当前状态清单

| area | 当前状态 |
| --- | --- |
| adopted production | ordered、source-indexed、dual-indexed、correspondence 的 `float` / dense / traits-gated xyz AoS path 已采纳；correspondence sorted-copy 有 size / disorder gate。 |
| custom layout evidence | Phase 055 两个 custom layout 样本 mixed；Phase 056 256K order-pattern profile positive with variance warnings；Phase 057 compact-ish / huge-padding sampled positive with padding sensitivity warnings。 |
| remaining gap | roadmap / matrix 仍明确禁止把当前 custom layout 证据外推到异常 alignment 或任意自定义点型。 |
| correctness freshness | 最近 Phase 058 后 `run_test_compare_recorded` 为 Std/RVV 各 20 tests passed，registry fresh。 |

## 假设与候选族

`custom-layout-alignment-sensitivity` 是证据扩展候选，不是新的 RVV 实现族。假设如下：

- `alignas(64)` / `alignas(32)` 点型会改变 `alignof(PointT)` 和 `sizeof(PointT)`，但 production RVV helper 只依赖 traits 解析出的 x/y/z offset 与 stride。
- 若 QEMU correctness 和 board repeated 仍 positive，当前 production gate 对“对齐较大的 registered xyz AoS 点型”有更强采样证据。
- 若 board 出现退化或 long-tail，只能说明该采样 slice 有风险，不能回推已采纳常见 PCL 点型或 Phase 057 的 sampled positive。

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | correctness / fallback target | bench / ablation target | board evidence | Evidence Doctor | decision gate |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `custom-layout-alignment-sensitivity` | source-indexed / dual-indexed / correspondence | `LocalAligned64XYZSource -> LocalAligned32XYZTarget` / `float` / registered xyz AoS / 64K + 256K | 新 gtest + current Std/RVV correctness | `custom-layout-alignment-sensitivity` QEMU smoke | 5-run board repeated；B/A = Std/RVV | QEMU + board Doctor | all positive => sampled positive with alignment warnings；否则按 row source / size 降级。 |

## 实现和测试动作

| action | 产物 / 命令 | 完成判据 |
| --- | --- | --- |
| A1 新增点型与 correctness | `include/impl/tesvd_scale_support.hpp`、`src/test_tesvd_scale.cpp` | static_assert 覆盖 offset、sizeof、alignof；public ordered 与三类 row-source reference 通过。 |
| A2 新增 bench case-filter | `src/bench_tesvd_scale.cpp` | 输出 6 个 row-source case：三类 row source × 64K/256K。 |
| A3 新增 Make / registry target | `Makefile` | QEMU smoke、board repeated、Doctor 和 registry 有独立目录与 run label。 |
| A4 脚本识别 | `script/generate_tesvd_scale_qemu_evidence_manifest.py`、`script/generate_tesvd_scale_board_repeated_summary.py` | manifest 中 evidence role / boundary / gate 不落到默认 diagnostic。 |
| A5 文档回填 | phase result、matrix、roadmap、topic-local docs | 当前证据与边界同步。 |

## Evidence Doctor 和 registry 规则

- QEMU output：`log/qemu/custom_layout_alignment_sensitivity/`
- Board output：`log/board/custom_layout_alignment_sensitivity_repeated/`
- QEMU run label：`qemu-tesvd-scale-custom-layout-alignment-sensitivity-smoke-phase059`
- Board run label：`custom_layout_alignment_sensitivity_repeated`
- evidence role：`production_public_custom_layout_alignment_sensitivity`
- raw logs 默认 local-only；summary / manifest / doctor 若被文档引用，可作为 summary-only 提交候选。

## 板卡复跑预算和决策桶

- run count：5
- iterations / warmup：沿用 topic 默认 `20 / 5`
- `positive`：每个 case 的 5-run B/A 全部大于 1.20。
- `weak_positive`：median >= 1.05 且 min >= 0.97。
- `negative`：median < 0.97 或 min < 0.97。
- `unstable`：方向摇摆且不满足上述桶。

## 继续 / 停止条件

本阶段完成后：

- 若全 case positive / weak-positive，更新为 sampled positive with alignment sensitivity boundary。
- 若存在 negative / unstable，只按 row source / size 降级，不触发 production rollback。
- `Scalar=double` 继续需要用户确认数值预算。
- 更广 layout / alignment 取样空间仍需用户定义采样空间与 board budget；本阶段只关闭一个测试本地采样 slice。
