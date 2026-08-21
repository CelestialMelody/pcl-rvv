# Phase 010 / PI1 计划：depth/disparity 生产接入边界

## 阶段意图和边界

本阶段是 PI1 production integration plan（生产接入计划），只冻结接入范围和验证计划，不修改 production（生产源码）。候选来自 phase 000 和 phase 020 的 positive production-shaped diagnostic（生产形态诊断）：

- `DepthImage::fillDepthImage()` 和 `fillDisparityImage()` 在 `xStep == 1`、`src_width == width`、`src_height == height` 的 contiguous（连续）路径上可接入 RVV。
- `DepthImage::fillDepthImage()` 和 `fillDisparityImage()` 在整数倍 downsample（下采样）且 `xStep > 1` 的路径上可作为 bounded production probe（有界生产探针）候选，但 phase 020 有 long-tail Warning，PI4 必须扩大 runs 或补环境 metadata。

本阶段不覆盖：

- `fillDepthImageRaw()`。
- `openni_camera/openni_depth_image.cpp` legacy 入口。
- public API（公开接口）签名变更。
- 非 RVV 构建行为变化。

## production patch 候选形态

| production 入口 | 标量 helper | RVV helper | dispatch / fallback |
| --- | --- | --- | --- |
| `DepthImage::fillDepthImage()` | 抽出原循环为 `fillDepthImageStd` 或邻近 internal helper | `__RVV10__` 下提供 contiguous `uint16_t -> float meters` helper；可同文件追加 downsample `vlse16` helper | 公开入口先执行现有尺寸异常检查；整数倍 full-size / downsample 且 `line_step` 归一后安全时尝试 RVV；其它路径回标量。 |
| `DepthImage::fillDisparityImage()` | 抽出原循环为 `fillDisparityImageStd` 或邻近 internal helper | `__RVV10__` 下提供 contiguous `constant / pixel` helper；可同文件追加 downsample `vlse16` helper | 同上；invalid pixel 写 0，`constant` 必须保持 `focal_length_ * baseline_ * 1000.0f / xStep`。 |

生产 helper 应放在 `io/src/image_depth.cpp` 中目标入口附近，保持 PCL 现有 `foo (bar)` 代码风格。production 注释只解释 contiguous gate、downsample fallback 和 invalid mask 语义，不复制测试说明。

## fallback 矩阵

| 条件 | 预期行为 | PI3 测试要求 |
| --- | --- | --- |
| 非 RVV 构建 | 完全走标量 helper | Std build correctness 通过。 |
| `__RVV10__` 启用且 contiguous 全尺寸 | 先走 RVV helper | RVV build direct test 命中 RVV 路径，输出与标量一致。 |
| `__RVV10__` 启用且整数倍 downsample | 可走 downsample RVV helper | direct test 覆盖 depth/disparity downsample；PI4 repeated board 单独报告 long-tail。 |
| 非整数 downsample | 保持 production 原异常行为 | public entry 现有检查先执行，不在 RVV helper 内吞掉异常。 |
| `line_step == 0` | 归一成 tight row，contiguous 可走 RVV | direct test 覆盖 zero line step。 |
| padded output row | RVV 每行只写 `width` 个 float，padding 不写 | direct test 验证 padding sentinel 保留。 |
| invalid `0/no_sample/shadow` | depth 写 NaN，disparity 写 0 | direct test 覆盖三类 invalid pixel。 |
| upsample | 保持 production 原异常行为 | public entry 现有检查先执行。 |

## PI2-PI5 验证计划

| PI 阶段 | 动作 | 命令 / 产物 | 完成判据 |
| --- | --- | --- | --- |
| PI2 production patch | 修改 `io/src/image_depth.cpp`，抽出 Std helper 并添加 `__RVV10__` RVV helper | production diff | 公开 API 不变，非覆盖路径自然 fallback；contiguous 和 downsample gate 均在源码中一眼可见。 |
| PI3 production direct tests | 在 topic 测试资产中新增真实 `DepthImage` wrapper direct test 或等价 production-entry harness | `make run_test_compare` | Std/RVV 都通过，且覆盖 contiguous、padded、downsample RVV、invalid pixel、zero line step 和异常 fallback。 |
| PI4 production evidence rerun | dump production-linked asm、板卡 repeated compare | production-specific asm dump；board repeated summary；Evidence Doctor | RVV 指令归属到 production helper 或可解释的内联范围；Milkv-Jupiter repeated board 无 Error；downsample long-tail 必须解释或扩大 runs。 |
| PI5 production evidence decision | 汇总 production direct 证据 | phase result、evaluation、Handoff | 无论正负都暂停在用户检查点，保留 patch，等待用户确认采纳或回滚。 |

## Evidence Doctor 和 registry 规则

PI4 必须生成 production direct 或 production-public evidence manifest，字段至少区分 `production_shaped_diagnostic` 与 `production-public`。如果仍使用 bench helper 而非真实 `DepthImage` entry，结论必须降级，不能写成 production evidence。当前 topic 已有 `log/evidence_registry.json`；production direct 重跑后必须新增或刷新对应 registry entry，并运行 `make evidence_status` 或 production-specific freshness check。downsample 的 phase 020 doctor 已有 Warnings=2，因此 production direct summary 不能只给总平均值，必须保留 per-case min/median/max。

## diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | PI1 后目标是 production-public 或 production-detail；phase 000 证据只作为 production-shaped diagnostic 输入。 |
| A/B boundary | PI4 必须让 baseline 和 candidate 经过同一个 production entry 或 production detail helper，不再只比较 test helper。 |
| 当前决策问题 | bounded production probe 是否在 contiguous 和整数倍 downsample depth/disparity 范围内成立。 |
| diagnostic 是否可外推到 production | 只支持接入计划，不支持最终采纳。真实 dispatch、异常路径和 wrapper 元数据必须由 PI3/PI4 证明。 |
| comparison-boundary / baseline mismatch 风险 | 存在；若 production direct harness 不能调用真实 `DepthImage`，必须明确降级并暂停。 |
| 弱 / 负 / 中性 / 不稳定时 bounded production probe | 若 production direct 不再 positive 或 Evidence Doctor 出 Error，停在 PI5，不自动采纳或回滚。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 当前无已采纳同族 RVV；不需要 RVV-vs-RVV family selection，但仍需要同一 production boundary 的 Std/RVV repeated board。 |

## 继续 / 停止条件

继续到 PI2 需要用户明确授权修改 `io/src/image_depth.cpp`。授权后默认连续推进 PI2-PI5，但不得扩大到 `fillDepthImageRaw()`、OpenNI legacy 或 public API。PI5 完成后必须等待用户确认保留 / 采纳或回滚 production patch。

当前默认恢复动作：在用户授权 production integration loop 后，从本计划进入 PI2 production patch。
