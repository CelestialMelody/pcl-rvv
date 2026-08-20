# Phase 060 plan: production detail color-gather ablation

## 阶段意图和边界

本阶段要验证一个新的 production-detail candidate family（生产细节候选族）：把窗口内 RGB 颜色距离和 RGB 查表从纯标量 staging 改成 RVV 通道内计算 + table gather（按索引表加载），观察它是否能比 phase 040/050 的 helper-only / nan-mask 变体更稳定地接近正向。

本阶段只改 `test-rvv/surface/bilateral_upsampling/src/bench_bilateral_upsampling.cpp` 和对应 topic-local 文档，不改 `surface/include/pcl/surface/impl/bilateral_upsampling.hpp`。当前生产补丁保持原样，仍处于用户判断点，不自动采纳也不自动回滚。

## 当前状态清单

- phase 020：direct-depth 诊断曾经正向，但未能直接支撑当前 production family。
- phase 030：public shell overhead ablation 仍负向。
- phase 040：helper-only bounded run 方向冲突，Evidence Doctor 出现 Errors。
- phase 050：NaN-only + k64 mask/chunk ablation 仍未形成稳定收益。
- 当前 production 头文件里仍保留 RVV 生产补丁，但没有 adopted。
- 当前 bench 侧已有 helper-only / nan-mask k64 两条 local helper 线，可作为对照基线。

## 假设与候选族

本阶段假设当前 helper-only / mask-chunk 线仍然被标量 RGB staging、RGB table lookup 和颜色差计算开销拖住。新的 candidate family 不再继续缩小 mask，而是尝试：

1. 让 RGB 距离在 RVV lane 内做。
2. 让 RGB weight 直接从查表里 gather。
3. 保留 depth load、finite mask、reduction 和 unprojection 的现有语义边界。

如果该族没有形成稳定正向，则它只能说明“颜色 staging 不是当前主瓶颈”或“当前实现形态仍不够好”，不能自动推出当前 production patch 应该 adopted。

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| production detail color-gather ablation | organized grid | `pcl::PointXYZRGB` / `pcl::PointXYZRGBA`, production AoS layout | topic-local bench helper direct call | 复用既有 scalar reference 对拍，确认 checksum 和数值误差不变 | board smoke 比较 color-gather RVV helper 与同边界 scalar | 以板卡 repeated smoke 为准 | 期望出现 `vlse8` / `vmaxu` / `vminu` / `vluxei16` / `vfredusum` | 需要 Evidence Doctor 解释载入、gather、边界和是否有 contract mismatch | planned | 先实现 bench-local helper，再跑 correctness、asm、board 和 doctor |

## 实现和测试动作

1. 在 `src/bench_bilateral_upsampling.cpp` 增加一个新的 bench-local RVV helper，核心变化是 RGB 距离向量化和 RGB table gather。
2. 保留当前 production helper 和 local nan-mask k64 helper，作为对照基线，不删除历史 case。
3. 跑 `run_test_compare`，确认测试支撑没有被改坏。
4. 跑 `dump_bench_rvv`，确认新 helper 的关键 RVV 指令能归属到目标符号。
5. 跑一次 `board_smoke`，只采信板卡结果，不用 QEMU 计时做性能判断。
6. 生成新的 evidence manifest，并跑 Evidence Doctor。

## Evidence Doctor 和 registry 规则

- 当前 topic 仍然没有可提交的 evidence registry，状态写 `evidence_registry_status=not_available`。
- 若 board summary 改变了当前 truth，旧 phase 050 结论仍视作历史基线，新 phase 060 result 需要单独记录。
- Evidence Doctor 出现 Error 时，先解释再决定是否保留该候选，不把单次 positive 当作 adopted。

## 阶段完成条件

- `attempted`：bench-local helper 完成，QEMU / asm / board / doctor 证据都跑通，但结果不够支持 production 接入。
- `rejected`：有板卡证据表明该 family 不值得继续。
- `deferred`：发现可继续的下一步候选，但本 phase 不再自动推进。
- `blocked`：需要用户判断、工具不可用或证据矛盾。

## 板卡复跑预算和决策桶

本阶段只给一次 board smoke 预算。若结果落在负向或近阈值区间，默认不自动复跑同一 family；只在 Evidence Doctor 或板卡输出暴露新的可解释变化时再考虑追加一次。

## 继续 / 停止条件

- 若 color-gather 仍负向或不稳定，则转入新的 candidate family 讨论，不把当前 helper 写成 adopted。
- 若 color-gather 稳定正向，再决定是否要把它翻译回 production helper 的实现形态。

## 文档更新清单

- phase 060 result
- optimization matrix
- optimization roadmap
- current handoff
- `doc/benchmark-and-evidence.zh.md`
- `doc/bilateral_upsampling-evaluation.zh.md`
- `doc/phases/README.zh.md`

## roadmap 同步动作

- 新增 `production detail color-gather ablation` 路线。
- 将 helper-only / nan-mask k64 的结论标成当前 family 的历史对照，而不是继续加深同一 family。

## diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | production-detail-ablation |
| A/B boundary | topic-local bench helper direct call |
| 当前决策问题 | 这个新 family 是否比 helper-only / nan-mask 更接近稳定收益 |
| diagnostic 是否可外推到 production | 不能直接外推，只能作为候选族筛选 |
| comparison-boundary / baseline mismatch 风险 | 有；bench helper 仍不是 production direct |
| weak / negative / neutral / unstable 时是否允许 bounded production probe | 允许本 phase 的 board smoke，但不自动进入 production 接入 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 是；如果后续要采纳，还需要同边界对照 |

## 当前默认恢复动作

先实现 color-gather bench helper，再用板卡证据判断它是不是一个值得继续的候选族。
