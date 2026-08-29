# Phase 020 Result: filter-5x5-rvv

## 当前状态

本阶段把 `SurfaceNormalModality<PointInT>::filterQuantizedSurfaceNormals()` 的 5x5
histogram filter（直方图滤波）推进到 production direct（真实生产路径证据）边界。
真实公开入口 `SurfaceNormalModality<PointXYZRGBA>::processInputData()` 现在先命中
`computeAndQuantizeSurfaceNormals2()` 的 RVV 路径，再在 `__RVV10__` 下尝试
`filterQuantizedSurfaceNormalsRVV()`；非 RVV 构建、force-scalar test hook（测试强制标量钩子）
或小规模输入仍回到标量路径。

本轮用户已授权：生产接入后的板卡结果只要显示有收益即可采纳。因此 Phase 020 的
production patch（生产补丁）在当前证据边界内视为 adopted production behavior
（已采纳生产行为）。

## 计划与实际

| action | status | evidence | note |
| --- | --- | --- | --- |
| RED / path hook | done | `test-rvv/recognition/surface_normal_modality/src/test_snm.cpp` | public entry 对拍期望 RVV build 最终记录 `FilterRvv`，防止只命中上一阶段 quantize RVV。 |
| production patch | done | `recognition/include/pcl/recognition/surface_normal_modality.h` | 抽出 `filterQuantizedSurfaceNormalsStd()`，新增 `filterQuantizedSurfaceNormalsRVV()` 和入口分流。 |
| fallback gate | done | 同上 | `width < 12 || height < 12` 回标量，保持边界和 5x5 邻域访问安全。 |
| correctness | done | `make -C test-rvv/recognition/surface_normal_modality run_test_rvv`、`run_test_compare` | Std/RVV public entry 对拍通过，small organized input fallback 通过。 |
| asm attribution（反汇编归属） | done | `make -B -C test-rvv/recognition/surface_normal_modality check_snm_production_rvv_asm` | full asm 可见 `filterQuantizedSurfaceNormals`，RVV asm 命中 `vle8/vse8/vmseq/vmsgtu`。 |
| board repeated（重复板卡性能测试） | done | `log/board/repeated_phase020_filter_5x5/summary.md` | 5-run，两个 production case median 都为 `1.420x`，`B/A < 1 = 0/5`。 |
| Evidence Doctor（证据体检） | done | `log/board/repeated_phase020_filter_5x5/evidence_doctor.md` | `Errors=0 / Warnings=0 / Suggestions=4`。 |
| evidence registry（证据登记表） | done | `log/evidence_registry.json` | Phase 020 summary / manifest / doctor 已登记为 `fresh`。 |

## 证据解释

| case | 入口与规模 | median speedup | range | 退化频率 | 结论 |
| --- | --- | ---: | --- | --- | --- |
| `production_process_320x240` | `processInputData()`，320x240 organized `PointXYZRGBA` | `1.420x` | `1.410x` - `1.430x` | `0/5` | positive |
| `production_process_641x481_tail` | `processInputData()`，641x481 tail | `1.420x` | `1.410x` - `1.420x` | `0/5` | positive |

checksum（校验和）在 Std/RVV 两侧一致。计时边界包含 depth-to-normal / quantize、5x5 filter
和 spread；不包含 `extractFeatures()`。因此这批证据证明“当前 public RVV path 快于当前
public scalar path”，不能证明 `spreadQuantizedMap()`、`extractFeatures()` 或其它模板点型也已完成。

Evidence Doctor 的 4 个 Suggestion 都是环境 metadata（环境元数据）和 binary identity
（二进制身份）缺失：缺少 taskset、governor、freq、temperature 和 binary hash。它们不阻塞
当前 positive 结论，因为两个 case 都没有 Error / Warning、没有 checksum mismatch（校验和不一致），
也没有 B/A 反向样本；后续板卡采集应补齐这些字段，便于解释温度、调频或旧 binary 混入风险。

## Diagnostic To Production Mismatch Audit

| question | answer |
| --- | --- |
| evidence role | `production_direct` |
| A/B boundary | `public_overload`，Std/RVV 都经 `SurfaceNormalModality<PointXYZRGBA>::processInputData()` |
| 当前决策问题 | 当前 public RVV path 是否快于当前 public scalar path |
| diagnostic 是否可外推到 production | 不需要外推；本阶段使用生产直连证据。 |
| comparison-boundary / baseline mismatch 风险 | 受控；同一公开入口、同一输入构造、同一 case-filter 和同一计时边界。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 已完成 bounded probe，结果 positive；若未来复跑反转，应先降级为 `stale_doc_pending_refresh` 再重跑同边界证据。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 不需要；本阶段不是 RVV-family-selection（RVV 实现族选择），而是新增 filter 子链路相对标量的 public-entry 采纳。 |

## 覆盖范围与未覆盖范围

| 范围 | 状态 | 说明 |
| --- | --- | --- |
| `PointXYZRGBA` organized public input | adopted | correctness、asm 和 repeated board 都覆盖真实公开入口。 |
| `Scalar=float` / 连续 AoS（结构数组）布局 | adopted | 当前 production case 的输入构造覆盖这一布局。 |
| 小规模输入 | scalar fallback | `width < 12 || height < 12` 时不进入 filter RVV。 |
| 非 RVV 构建 | scalar fallback | `__RVV10__` 未启用时只编译 Std helper。 |
| 其它模板点型 / layout | unvalidated | 当前没有 point-type expansion（点类型扩展）证据，不能外推。 |
| `QuantizedMap::spreadQuantizedMap()` | deferred | 仍是独立未验证 candidate。 |
| `extractFeatures()` | out of scope | 当前 production bench 不覆盖特征抽取。 |

## 阶段反思

Phase 020 把 public entry speedup 从 Phase 010 的 weak-positive 提升到两个 case 都 `1.420x`。
这说明 5x5 filter 是当前 `processInputData()` 中值得保留的 RVV 子链路。下一阶段最自然的候选是
`QuantizedMap::spreadQuantizedMap()`：它仍在同一个 public entry 的计时边界内，若能保持 checksum
一致并在板卡上继续正向，可以进一步提高当前 adopted production behavior。

继续前必须另写 phase plan，原因是 spread 的语义和 filter 不同：它按 spreading radius 写回邻域，
会引入更明显的 scatter/write amplification（写放大）风险，不能沿用本阶段 5x5 histogram 的证据。

## EvidenceDecision

`filter-5x5-rvv` 在当前 Phase 020 边界下判定为 `adopted production behavior`。

可采纳理由：

- correctness（正确性）对拍通过，RVV build 命中 `FilterRvv` hook。
- asm attribution 能看到 filter 相关 RVV byte load / compare / store 指令。
- repeated board 的两个 production case 都为 positive，median `1.420x`，无退化样本。
- Evidence Doctor 无 Error / Warning，Suggestion 不改变当前结论。

## 下一步

默认下一 phase 是 `030-spread-quantized-map-rvv`。若继续当前 topic，应先为
`QuantizedMap::spreadQuantizedMap()` 写 phase plan，冻结 spread 半径、写回边界、fallback gate、
correctness、asm、board repeated 和 Evidence Doctor 输入。若不继续，则当前 topic 可停在
“depth quantize + 5x5 filter 已采纳、spread / extractFeatures 未覆盖”的生产状态。
