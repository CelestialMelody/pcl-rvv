# Phase 010 plan: normalize component diagnostic

## 阶段意图和边界

本阶段做 `normalizeHistogram` 的 component ablation（组件消融）：用 test-only helper（测试专用 helper）复刻 production 中 descriptor L2 归一化的数学语义，再实现一个 RVV path（RVV 执行链路）候选，验证 352 / 1344 维连续数组是否值得进入后续 production probe（生产探针）。

本阶段不修改 `features/include/pcl/features/impl/shot.hpp`，不改变公开 API，不接入 production dispatch（生产分流），不覆盖 `interpolateSingleChannel` / `interpolateDoubleChannel` 的 histogram scatter（直方图离散写入）语义。

`validated_scope`：`float*` descriptor buffer，长度 352 和 1344；有限非零输入；同构链路 L2 norm = `sqrt(sum(x*x))` 后逐元素除以 norm；synthetic descriptor batches；test-only bench。

`unvalidated_scope`：零 norm 或含 NaN descriptor 的 production 行为、Eigen::VectorXf 对象内部布局、真实 `computePointSHOT` 调用路径、其它 descriptor length、`Scalar=double`、production fallback / dispatch、public-entry end-to-end speedup。

`phase_closeout_boundary`：本阶段最多关闭 normalization component diagnostic 的 correctness / asm / board A/B；不能关闭 SHOT public production performance 或 clean adoption。

## 当前状态清单

| area | current state | path |
| --- | --- | --- |
| Phase 000 | public fixed-LRF scaffold 已通过 correctness、asm dump 和 board smoke；Evidence Doctor 因单次退化和 run count 低而降级性能结论。 | `doc/phases/000-current-state-and-diagnostic-plan/result.zh.md` |
| production source | `normalizeHistogram` 是两段顺序循环：先 double accumulator 累加 `shot[j] * shot[j]`，再 `sqrt`，最后 float 除法写回。 | `features/include/pcl/features/impl/shot.hpp` |
| current tests | 已覆盖公开入口 descriptor unit norm；尚未有 component-level normalization 对拍。 | `src/test_shot.cpp` |
| current bench | 只有 public SHOT fixed-LRF case；尚未有 normalization component case-filter。 | `src/bench_shot.cpp` |
| evidence registry | 尚未接入 registry；继续使用 manifest + doctor + 人工 freshness scan。 | `log/board/evidence_manifest.json` |

## 假设与候选族

| candidate family | hypothesis | risk |
| --- | --- | --- |
| normalize descriptor RVV | descriptor 连续数组可用 RVV 加速平方和与除法写回，1344 维比 352 维更可能受益。 | `sqrt` 只执行一次，public entry 可能仍被 search/interpolation 稀释；double accumulator 语义和 RVV float reduction 有误差风险。 |
| scalar exact reference | 按 production 当前顺序用 double 累加、float 写回，作为 same-chain correctness reference。 | 如果后续生产源码改变，该 test-only reference 需要刷新。 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| descriptor normalization / copy RVV | contiguous descriptor array | `float`, lengths 352 / 1344 | test-only `normalizeDescriptorRVV` | `run_test_compare` 新增 normalization tests | `normalize_352_component` / `normalize_1344_component` | `board_smoke` 或细分 case-filter | `dump_bench_rvv` 检查 helper 指令 | manifest / doctor after board | planned |

## 实现和测试动作

| action | artifact / command | expected evidence | completion criterion |
| --- | --- | --- | --- |
| B1 red test | 修改 `src/test_shot.cpp`，先引用尚不存在的 normalization helper | 编译失败，证明测试会捕捉缺失候选。 | `make -C test-rvv/features/shot run_test_compare` 在缺 helper 时失败。 |
| B2 helper implementation | 新增 `include/impl/shot_normalize.hpp` 并从 `include/shot.h` 聚合 | scalar reference 与 RVV candidate 可编译。 | Std/RVV correctness 通过；非 RVV 构建 fallback 到 scalar。 |
| B3 component bench | 扩展 `src/bench_shot.cpp` 和 case-filter | component timing、checksum 和 case label 可解析。 | 板卡 compare 能看到 normalization case。 |
| B4 asm | `make -C test-rvv/features/shot dump_bench_rvv` | RVV load/mul/reduction/div/store 能归因到 normalization helper 或 bench callsite。 | 若只看到自动向量化或无法归因，降级说明。 |
| B5 board evidence | `make -C test-rvv/features/shot board_smoke`，必要时使用 case-filter 细跑 | correctness + component A/B + Evidence Doctor。 | 板卡可用时完成；若 doctor 有 Error / Warning，写入 result 并降级边界。 |
| B6 docs | Phase 010 result、matrix、roadmap、evaluation、README | 可恢复 evidence decision。 | 每个动作回填 done / partial / deferred / blocked。 |

## Evidence Doctor 和 freshness 规则

本阶段继续用 topic-local `script/generate_shot_evidence_manifest.py` 生成 manifest，再运行：

```bash
make -C test-rvv/features/shot run_evidence_doctor
```

若新增 component case 后 manifest 脚本还未能解析 case label，必须先修脚本再使用 doctor 输出。若 doctor 报 `ba_degradation_frequency`、`low_run_count` 或 asm 边界缺口，结果只能作为 component diagnostic，不能写 production-ready。

## 板卡复跑预算和决策桶

默认先执行一次 `board_smoke`。若 normalization component case 出现 `>1.10x`，追加最多 2 次同 case-filter 复跑确认方向；`1.02x-1.10x` 记为 weak_positive 并优先考虑 public-entry 稀释风险；接近 1 或低于 1 记为 neutral / negative，不进入 production integration loop。

## Diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | component diagnostic。 |
| A/B boundary | test helper / component bench；不是 public overload，也不是 production detail helper。 |
| 当前决策问题 | implementation-shape 和 RVV-vs-scalar component A/B。 |
| diagnostic 是否可外推到 production | limited。只能说明 normalization 这段连续数组循环是否值得 production probe；不能证明公开入口整体变快。 |
| comparison-boundary / baseline mismatch 风险 | yes。component helper 的数据来自 synthetic descriptor batch，不含 search、LRF、interpolation 和 Eigen::VectorXf 对象状态。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | normally no。只有 correctness 稳定且 1344 维 repeated board 明显 positive，才允许进入窄 production probe。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | yes。production 接入前必须补真实 `normalizeHistogram` 或 public entry detail A/B。 |

## 继续 / 停止条件

默认继续到 B1-B6。只有出现以下情况才停止：新增测试无法在当前 topic 内表达、交叉工具链或板卡不可用、component correctness mismatch 无法修复、继续需要修改 production、或 dirty isolation 变得不安全。

Phase 010 若 negative / neutral，下一阶段默认回到 `createBinDistanceShape` 或 interpolation 前置 component diagnostic；若 positive，下一阶段进入 bounded production probe planning（有界生产探针计划），但仍需用户确认生产接入边界。
