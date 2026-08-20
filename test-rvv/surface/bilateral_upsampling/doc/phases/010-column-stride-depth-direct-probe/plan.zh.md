# Phase 010: column-stride depth direct probe

## 阶段意图和边界

Phase 000 证明 staged-window-reduction correctness 通过但板卡负向，主要风险是 per-pixel staging 抵消规约收益。本阶段尝试 `column-stride-depth-direct`：对固定 `x_w` 的窗口列，用 `vlse32.v` 直接按 organized row stride 读取 depth `z`，只暂存标量算出的 RGB/depth table weight（权重），避免另建 depth staging 数组。

本阶段仍只修改 `test-rvv/surface/bilateral_upsampling`，不修改 production（生产源码）。它仍是 diagnostic（诊断），不是 production direct（真实生产路径证据）。

validated_scope：test-local `RgbPoint`、organized grid、float depth、uint8 RGB、`window_size=3/4/5`、dense 与 NaN holes。

unvalidated_scope：真实 `PointXYZRGB/RGBA` layout、真实 `performProcessing` dispatch、RGB table gather、production fallback。

## 候选族

| candidate | 与 Phase 000 差异 | 预期 | 风险 |
| --- | --- | --- | --- |
| column-stride-depth-direct | depth 不再 staging；每列用 `vlse32.v` 跨 row 读取 z，并用 NaN mask 清零无效 lane。 | 降低 staging 内存写回，保留窗口列组织。 | RGB weight 仍标量暂存；短列 VL 小，`vlse32` 和 vfred overhead 可能仍大。 |

## 实现和测试动作

| action | 产物 / 命令 | 完成判据 |
| --- | --- | --- |
| candidate | `processColumnStrideDepthRVV` / `processDirectDepthCandidate` | 编译通过，未触碰 production。 |
| correctness | `make -C test-rvv/surface/bilateral_upsampling run_test_compare` | 新增 direct-depth correctness 通过。 |
| asm | `make -C test-rvv/surface/bilateral_upsampling dump_bench_rvv` | 看到 `vlse32.v`、`vfmul.vv`、`vfredusum.vs` 可归到候选。 |
| board | `make -C test-rvv/surface/bilateral_upsampling board_smoke` | direct-depth case 生成板卡 compare summary。 |
| doctor | 更新 manifest 并运行 Evidence Doctor | 解释退化、正向或边界 warning。 |

## 板卡复跑预算和决策桶

沿用 phase 000：先跑一次 `board_smoke`；若 direct-depth 接近阈值或跨方向，最多追加一次同边界复跑。`positive >= 1.10x`，`weak_positive 1.03x..1.10x`，`neutral 0.97x..1.03x`，`negative < 0.97x`。

## 继续 / 停止条件

若 direct-depth 仍 negative / neutral，则不建议接入 production，并关闭当前 topic 的低风险候选。若 stable positive / weak_positive，则停在用户确认点，建议是否进入 PI1 production integration plan。

