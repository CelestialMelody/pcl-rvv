# Phase 060 RVV Bin Index Precompute Plan

## 阶段意图和边界

本阶段在 Phase 050 production RVV 形态上继续做一个窄优化：把每个向量通道（lane，RVV 向量寄存器中的一个元素位置）
的 histogram bin（直方图箱号）计算从标量 `std::floor` / clamp 转为 RVV 预计算，再把每个
chunk 生成的 `int32` bin 暂存到 O(VLmax) 小缓冲。最终 histogram increment（直方图加一类更新）
仍按 lane 顺序标量执行，避免同 bin 冲突和浮点累加顺序变化。

本阶段不扩大生产入口、点类型、`Scalar`、indices、`normalize_bins=false`、`size_component` 或 `normalize_distances`
范围。覆盖边界与 Phase 050 相同，只验证 `production_vfh_compute_default` 是否继续稳定优于标量，
以及是否优于 Phase 050。

## 当前状态清单

| item | current state |
| --- | --- |
| Phase 050 production shape | chunk-local staging 已通过 QEMU correctness、asm 和 board repeated。 |
| Phase 050 production board | 5-run checksum 全一致；`production_vfh_compute_default` mean Std `8.80421 ms`，RVV `6.10923 ms`，mean speedup `1.44115x`。 |
| Evidence Doctor | Phase 050 为 `0E/0W/11S`；suggestion 只要求补环境 metadata / binary identity。 |
| 仍可优化点 | 每点仍有 4 次标量 `std::floor` / clamp 分箱；这些分箱处在每点热点 loop 中。 |

## 候选族与语义假设

| candidate family | hypothesis | numeric boundary | evidence needed |
| --- | --- | --- | --- |
| `vfh-production-rvv-bin-index-precompute` | RVV 预计算 bin index 能减少标量分箱成本，保留标量 histogram scatter 的确定顺序。 | 先把 scaled bin 值 clamp 到 `[0, bins - 1]`，再使用 `vfcvt.rtz.x.f.v`。由于转换前已非负，rtz 与 `floor` 等价；超过范围的值已被 clamp 到最终边界。 | QEMU correctness、asm 中 `vfcvt.rtz.x.f.v` 归属、5-run board repeated、Evidence Doctor。 |

## 实现和测试动作

| action | 产物 | 验收 |
| --- | --- | --- |
| IMPL | 在 `features/include/pcl/features/impl/vfh.hpp` 增加 vector bin helper，并让 SPFH/viewpoint RVV helper 写 `int32` bin buffer。 | 不修改 fallback gate；直方图更新仍按 lane 顺序标量执行。 |
| CORRECTNESS | `make -C test-rvv/features/vfh run_test_compare`。 | 当前 post-review 验证为 Std/RVV 各 9 个测试通过。 |
| ASM | `make -B -C test-rvv/features/vfh dump_bench_rvv`。 | 生产 helper 区域可见 `vfcvt.rtz.x.f.v` 或等价转换指令，原 RVV math/reduction 仍存在。 |
| BOARD | `board_repeated` 使用 `log/board/repeated-production-phase060-postreview` 作为当前生产接入后板卡证据路径。 | 5-run checksum 一致，production speedup 与 Phase 050 比较后决定是否保留。 |
| DOCTOR | Evidence Doctor 使用 Phase 060 manifest。 | `Errors=0`；Warning / Suggestion 已解释或降级。 |
| DOC | 更新 Phase 050/060 result、matrix、roadmap、evaluation、正式 `doc-rvv` 和筛选清单。 | 采用的生产数据来自最终保留的接入后板卡 run。 |

## Diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | `production-public`。 |
| A/B boundary | 真实 `VFHEstimation::compute()` public overload，Std build 对 RVV build；与 Phase 050 进行跨 run 实现形态比较。 |
| 当前决策问题 | `implementation-shape`：是否用 RVV bin index precompute 替代 Phase 050 的标量分箱。 |
| diagnostic 是否可外推到 production | 不外推；用 production direct board 结果。 |
| comparison-boundary / baseline mismatch 风险 | Phase 050 和 Phase 060 是不同 run 目录，可能受板卡波动影响；若差异很小，按简单性和稳定性决定。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 本阶段已经是 bounded production probe；若退化或收益不明显，恢复 Phase 050。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 若 Phase 060 明显优于 Phase 050 且 correctness / Doctor 闭合，可保留；若只是噪声内持平，优先保留代码更简单的 Phase 050。 |

## 板卡复跑预算和决策桶

预算为 5-run repeated，参数：
`BENCH_ARGS='--side 80 --iterations 8 --warmup 2'`。

- `positive`：checksum 全一致，平均 speedup 不低于 `1.2x`，并较 Phase 050 有清楚收益。
- `neutral`：checksum 全一致，仍优于标量但与 Phase 050 差异落在噪声内。
- `negative`：低于 Phase 050 明显或低于 `1.05x`。
- `unstable`：checksum 不一致、方向摇摆或 Evidence Doctor Error。

## 继续 / 停止条件

若 Phase 060 为 `positive`，保留 bin index precompute 并刷新生产文档；若为 `neutral` 或
`negative`，恢复 Phase 050，记录本路线为 attempted / rejected。若证据稳定且 roadmap 中只剩
需要扩大点类型或参数范围的动作，则停止在当前生产 closeout。
