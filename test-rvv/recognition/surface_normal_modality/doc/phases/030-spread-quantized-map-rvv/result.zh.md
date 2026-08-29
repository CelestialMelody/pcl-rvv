# Phase 030 Result: spread-quantized-map-rvv

## 当前状态

本阶段把 `SurfaceNormalModality<PointInT>::processInputData()` 末尾的 spread step（扩散步骤）
推进到 surface-normal 专用 production direct（真实生产路径证据）边界。实现没有修改公共
`QuantizedMap::spreadQuantizedMap()`，避免影响 color modality、color-gradient modality 和其它调用方；
只在 `SurfaceNormalModality` 内新增 `spreadFilteredQuantizedSurfaceNormals*()` 分流。

当前 `processInputData()` 的 adopted production behavior（已采纳生产行为）覆盖：

- `computeAndQuantizeSurfaceNormals2()` 的 depth-to-normal / quantize RVV。
- `filterQuantizedSurfaceNormals()` 的 5x5 histogram filter RVV。
- 默认 `spreading_size_=8` 时的 surface-normal 专用 spread RVV。

非 RVV 构建、force-scalar test hook（测试强制标量钩子）、小规模输入和非默认 spreading size
仍回到标量路径。

## 计划与实际

| action | status | evidence | note |
| --- | --- | --- | --- |
| RED / path hook | done | `make -C test-rvv/recognition/surface_normal_modality run_test_rvv` | 修改测试后，RVV build 因只记录到 `FilterRvv` 而失败，证明新测试能捕捉 spread RVV 缺失。 |
| production patch | done | `recognition/include/pcl/recognition/surface_normal_modality.h` | 新增 `spreadFilteredQuantizedSurfaceNormals()`、Std helper 和 RVV helper；`processInputDataFromFiltered()` 保持公共标量 helper。 |
| fallback gate | done | `test_snm.cpp` 的 `NonDefaultSpreadSizeKeepsScalarSpreadPath` | 非默认 `spreading_size=6` 时 RVV build 仍停在 `FilterRvv`，spread 回标量。 |
| correctness | done | `run_test_rvv`、`run_test_compare` | Std/RVV public entry 对拍通过，quantized / spreaded map 和 orientation map 一致。 |
| asm attribution（反汇编归属） | done | `make -B -C test-rvv/recognition/surface_normal_modality check_snm_production_rvv_asm` | full asm 可见 spread helper 或公共 spread 符号，RVV asm 命中 byte OR / store 指令。 |
| board repeated（重复板卡性能测试） | done | `log/board/repeated_phase030_spread_rvv/summary.md` | 5-run，median `1.620x` / `1.640x`，`B/A < 1 = 0/5`。 |
| Evidence Doctor（证据体检） | done | `log/board/repeated_phase030_spread_rvv/evidence_doctor.md` | `Errors=0 / Warnings=0 / Suggestions=4`。 |
| evidence registry（证据登记表） | done | `log/evidence_registry.json` | Phase 030 summary / manifest / doctor 已登记为 `fresh`。 |

## 证据解释

| case | 入口与规模 | median speedup | range | 退化频率 | 结论 |
| --- | --- | ---: | --- | --- | --- |
| `production_process_320x240` | `processInputData()`，320x240 organized `PointXYZRGBA` | `1.620x` | `1.620x` - `1.640x` | `0/5` | positive |
| `production_process_641x481_tail` | `processInputData()`，641x481 tail | `1.640x` | `1.630x` - `1.650x` | `0/5` | positive |

checksum（校验和）在 Std/RVV 两侧一致。计时边界包含 depth-to-normal / quantize、5x5 filter
和 spread；不包含 `extractFeatures()`。Phase 030 相比 Phase 020 的 public-entry speedup
从 `1.420x` / `1.420x` 提升到 `1.620x` / `1.640x`，支持保留 spread RVV。这个增量对比是
跨 phase 参考，不是同一二进制内的 RVV-vs-RVV detail A/B；最终采纳依据仍是 Phase 030 的
production direct Std/RVV repeated board 证据。

Evidence Doctor 的 4 个 Suggestion 仍是环境 metadata（环境元数据）和 binary identity
（二进制身份）缺失：缺少 taskset、governor、freq、temperature 和 binary hash。它们不阻塞
当前 positive 结论，因为两个 case 都没有 Error / Warning、没有 checksum mismatch（校验和不一致），
也没有 B/A 反向样本。

## Diagnostic To Production Mismatch Audit

| question | answer |
| --- | --- |
| evidence role | `production_direct` |
| A/B boundary | `public_overload`，Std/RVV 都经 `SurfaceNormalModality<PointXYZRGBA>::processInputData()` |
| 当前决策问题 | 当前 public RVV path 是否快于当前 public scalar path，以及 surface-normal 专用 spread RVV 是否可保留 |
| diagnostic 是否可外推到 production | 不需要外推；本阶段使用生产直连证据。 |
| comparison-boundary / baseline mismatch 风险 | 受控；同一公开入口、同一输入构造、同一 case-filter 和同一计时边界。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 已完成 bounded probe，结果 positive；若未来复跑反转，应先降级为 `stale_doc_pending_refresh` 再重跑同边界证据。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 当前不是新 family selection；Phase 030 只把同一 public entry 下仍为标量的 spread 子链路接入 RVV。跨 phase 增量仅作趋势说明。 |

## 覆盖范围与未覆盖范围

| 范围 | 状态 | 说明 |
| --- | --- | --- |
| `PointXYZRGBA` organized public input | adopted | correctness、asm 和 repeated board 都覆盖真实公开入口。 |
| `Scalar=float` / 连续 AoS（结构数组）布局 | adopted | 当前 production case 的输入构造覆盖这一布局。 |
| 默认 `spreading_size_=8` | adopted | RVV helper 只在默认 spread 8 下启用。 |
| 非默认 spreading size | scalar fallback | 例如 `spreading_size=6` 的测试确认 spread 不进 RVV。 |
| 小规模输入 | scalar fallback | 由 quantize / filter / spread gate 共同保持安全边界。 |
| 公共 `QuantizedMap::spreadQuantizedMap()` | not modified | 其它 modality 调用方不受本阶段生产补丁影响。 |
| 其它模板点型 / layout | unvalidated | 当前没有 point-type expansion（点类型扩展）证据，不能外推。 |
| `extractFeatures()` | out of scope | 当前 production bench 不覆盖特征抽取。 |

## 阶段反思

Phase 030 证明了 spread 是 `processInputData()` 当前 public-entry 计时边界里的有效优化点。
继续把公共 `QuantizedMap::spreadQuantizedMap()` RVV 化可能惠及其它 modality，但那会扩大到多个
production 调用方，已经超出本 topic 的授权和证据边界；应另开跨 modality topic，至少覆盖
color modality、color-gradient modality 和 surface-normal modality 的 correctness / fallback / board。

当前 topic 内没有新的未阻塞热点值得继续推进：`extractFeatures()` 不在当前 bench 计时边界内，且由
distance map、list sort 和 feature selection 状态主导；其它模板点型和非默认 spread size 属于扩展验证，
不是当前 adopted path 的必要采纳条件。

## EvidenceDecision

`snm-spread-rvv-2pass` 在当前 Phase 030 边界下判定为 `adopted production behavior`。

可采纳理由：

- correctness（正确性）对拍通过，RVV build 命中 `SpreadRvv` hook，非默认 spread size fallback 通过。
- asm attribution 能看到 spread 相关 RVV byte OR / store 指令。
- repeated board 的两个 production case 都为 positive，median `1.620x` / `1.640x`，无退化样本。
- Evidence Doctor 无 Error / Warning，Suggestion 不改变当前结论。

## 下一步

当前 topic 建议停在 “depth quantize + 5x5 filter + default spread 已采纳” 状态。若继续优化，
建议另开两个独立方向：

- 公共 `QuantizedMap::spreadQuantizedMap()` RVV 化：跨 modality topic，需要覆盖所有调用方。
- `extractFeatures()` profile-first（先做性能剖析）：只有完整 LINEMOD depth modality profile 证明它是热点时再启动。
