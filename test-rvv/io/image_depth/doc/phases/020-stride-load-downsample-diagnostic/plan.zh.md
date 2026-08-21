# Phase 020 计划：stride-load downsample diagnostic

## 阶段意图和边界

本阶段只在 test-rvv（测试资产）里尝试 `xStep > 1` 的 downsample RVV candidate（下采样 RVV 候选），不修改 `io/src/image_depth.cpp`。目标是判断 `vlse16`（跨步加载 16-bit 元素）能否让 `fillDepthImage()` / `fillDisparityImage()` 的整数倍下采样路径形成可归因收益。

本阶段覆盖：

- `fillDepthImage` production-shaped helper，`src_width / width > 1`，`src_height / height` 为整数。
- `fillDisparityImage` production-shaped helper，同一输入边界。
- output tight row 和现有 padding 语义；padding 不作为本阶段性能主 case。

本阶段不覆盖：

- production direct（真实生产入口分流）。
- 非整数 downsample 或 upsample 的异常路径。
- OpenNI legacy `openni_camera/openni_depth_image.cpp`。
- `fillDepthImageRaw()`。

## 当前状态清单

| 项目 | 当前状态 | 路径 |
| --- | --- | --- |
| phase 000 | contiguous depth/disparity 为 `partial-production-candidate` | `000-current-state-and-diagnostic-scaffold/result.zh.md` |
| PI1 | 已写计划，PI2 需要用户授权 production 修改 | `010-production-integration-plan/plan.zh.md` |
| downsample | 目前 RVV build 下仍回到 scalar fallback | `include/image_depth.h` |
| bench case | 已有 depth/disparity downsample case | `src/bench_image_depth.cpp` |

## 假设与候选族

| candidate family | 假设 | 风险 | 本阶段动作 |
| --- | --- | --- | --- |
| `vlse16` downsample depth | 对 `xStep > 1` 的源行用 `vlse16` 跨步加载，仍可批量 mask / convert / multiply | 跨步加载可能比标量更慢；每行 `vsetvl` 和非连续访问可能增加成本 | TDD 增加路径选择测试，实现 depth downsample RVV，跑 QEMU、asm、board |
| `vlse16` downsample disparity | 同上，并把 `constant` 保持为 `focal_length * baseline * 1000 / xStep` | 向量除法 + stride load 可能吞吐受限 | TDD 增加路径选择测试，实现 disparity downsample RVV，跑 QEMU、asm、board |
| scalar fallback | 非 RVV build、非整数 downsample、非覆盖路径仍走 scalar | fallback gate 不能误放大范围 | Std build compare 和 downsample correctness 保护 |

## 优化矩阵

| candidate family | row source / layout | scope and entry | correctness / fallback target | bench / board target | asm boundary | doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- |
| downsample depth `vlse16` RVV | wrapper-backed `uint16_t` image，source stride by `xStep` | production-shaped `fillDepthImage` helper，`640x480 -> 320x240` | `run_test_compare`，新增 path-selection test | `depth_downsample_640x480_to_320x240` repeated board | `dump_bench_rvv`，查 `vlse16` | planned | planned |
| downsample disparity `vlse16` RVV | 同上 | production-shaped `fillDisparityImage` helper，`640x480 -> 320x240` | `run_test_compare`，新增 path-selection test | `disparity_downsample_640x480_to_320x240` repeated board | `dump_bench_rvv`，查 `vlse16` / `vfrdiv` | planned | planned |

## 实现和测试动作

| 动作 | 产物 / 命令 | 完成判据 |
| --- | --- | --- |
| RED path-selection test | 修改 `src/test_image_depth.cpp`，先运行 `make run_test_rvv` | RVV build 因缺少 `CandidatePath` / selector 编译失败或断言失败。 |
| GREEN downsample helper | 修改 `include/image_depth.h` | `make run_test_compare` 通过，Std build 仍选择 scalar，RVV build 选择 downsample RVV。 |
| 反汇编归属 | `make dump_bench_rvv` | asm 中出现 `vlse16`，并仍有 mask / convert / store 指令。 |
| 板卡 repeated | 复跑 downsample depth/disparity，生成 run-labelled summary / manifest / doctor | Evidence Doctor 无 Error；若 negative/neutral，用证据拒绝或暂缓。 |
| 文档同步 | 更新 result、matrix、roadmap、evaluation、registry | 结论不外推到 production direct。 |

## Evidence Doctor 和 registry 规则

若本阶段形成 board repeated summary，应放到 `log/board/repeated_downsample/`，并生成 `summary.md`、`evidence_manifest.json`、`evidence_doctor.md`。若把它作为当前结论引用，必须登记到 `log/evidence_registry.json`，并运行 freshness check。phase 000 的 repeated contiguous 证据保持独立，不混入 downsample decision。

## 板卡复跑预算和决策桶

板卡当前可用。本阶段预算为 downsample depth/disparity 各 5-run，每次 `iterations=10`、`warmup=2`。decision bucket：

- `positive`: median speedup `> 1.10x` 且 min `>= 1.02x`。
- `weak_positive`: median `1.02x-1.10x` 或 min 低于 `1.02x` 但方向多数正向。
- `neutral`: median `0.98x-1.02x`。
- `negative`: median `< 0.98x`。
- `unstable`: 正负方向摇摆或 Evidence Doctor long-tail warning 影响 bucket。

## diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | production-shaped diagnostic |
| A/B boundary | test helper；baseline 是 scalar reference，candidate 是 test-rvv RVV helper |
| 当前决策问题 | `vlse16` downsample RVV 是否值得保留为后续 production candidate |
| diagnostic 是否可外推到 production | 只能外推到“是否值得未来生产探针”；不能直接接入 production。 |
| comparison-boundary / baseline mismatch 风险 | 有；helper 不含真实 `DepthImage` dispatch 和异常路径。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 若结果不是 positive，默认不进入 production probe，除非后续 profile 证明 downsample 是主成本且用户授权。 |
| clean adoption 是否需要 production boundary 证据 | 需要；本阶段不能 clean adopt。 |

## 继续 / 停止条件

本阶段默认继续到 QEMU correctness、asm 和板卡 repeated。若 `vlse16` 结果 negative 或 unstable，记录为 rejected/attempted，并把 production path 保持在 contiguous-only PI1；若 positive，可把 downsample 加入后续 PI1 扩展候选，但仍需用户授权 production 修改。
