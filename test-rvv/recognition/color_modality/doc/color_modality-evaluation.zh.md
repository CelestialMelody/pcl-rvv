# color_modality Evaluation

## 范围和目标源码

目标源码是 `recognition/include/pcl/recognition/color_modality.h`。当前生产采用窄范围 production RVV，接管 `ColorModality<PointXYZRGB>::processInputData()` 中的 RGB extrema quantize 和 3x3 dominant filter，再调用既有 `QuantizedMap::spreadQuantizedMap()`。`extractFeatures()` 和 `computeDistanceMap()` 仍保持标量。

## 函数 / 函数族作用速览

| 函数 / 函数族 | 作用 | 输入 / 输出状态 | 与主流程关系 | RVV 判断 |
| --- | --- | --- | --- | --- |
| `processInputData()` | 公开入口，组织 quantize、filter 和 spread | input cloud -> quantized / filtered / spreaded maps | 生产 direct 计时边界 | adopted |
| `processInputDataStd()` | 标量回退事实来源 | same input -> same output maps | RVV fallback | adopted |
| `quantizeColorsRVV()` | RGB extrema 量化 RVV helper | organized `PointXYZRGB` -> `quantized_colors_` | 生产 direct 的前置阶段 | adopted |
| `filterQuantizedColorsRVV()` | 3x3 dominant filter RVV helper | `quantized_colors_` -> `filtered_quantized_colors_` | 生产 direct 的核心收益点 | adopted |
| `extractFeatures()` | 从 filtered map 生成候选特征 | mask -> features | 当前不在计时边界 | deferred until profile |
| `computeDistanceMap()` | 生成距离图 | mask -> distance map | 当前由 `extractFeatures()` 消费 | deferred until profile |

## 函数级结论

当前决定是 `production-candidate` 变成 `adopted production behavior`。Phase 040 板卡 repeated 显示 `production_process_320x240` median `2.490x`、`production_process_641x481_tail` median `2.410x`，两组 `B/A < 1` 都是 `0/5`，checksum 稳定。相对 Phase 030 的公开入口 repeated baseline（约 `1.38x` / `1.36x`）明显更高，因此当前 quantize+filter family 采纳为长期生产行为。

## 标量流程与 RVV 流程对照

标量流程保持原意：

1. `quantizeColors()` 逐像素读取 `r/g/b`，做 8-bin extrema 分类。
2. `filterQuantizedColors()` 对 3x3 邻域做 histogram，严格 `>` 维持最早 bin tie-break。
3. `QuantizedMap::spreadQuantizedMap()` 传播 filtered map。

RVV 流程只接管前两步：

1. `quantizeColorsRVV()` 只对 exact `PointXYZRGB` 和 organized cloud 走 RVV，使用 `vlse8` 读取 AoS 中的 `r/g/b` 字节，按整数缩放复刻标量距离比较和 tie-break。
2. `filterQuantizedColorsRVV()` 在 quantized byte map 上做 3x3 邻域计数，`vmseq/vmsgtu/vmerge` 保持标量同样的最早 bin 选择。
3. spread 保持既有 `QuantizedMap::spreadQuantizedMap()`。

## 实现方式审计

| 实现维度 | 当前状态 | 证据 | 边界 / 恢复条件 |
| --- | --- | --- | --- |
| dispatch / fallback | adopted | `run_test_compare`、board repeated、`processInputDataStd()` | 非 RVV 构建、强制标量 hook 和不满足 gate 的输入回退标量 |
| layout / traits gate | adopted | exact `PointXYZRGB`、organized 输入、AoS stride | 其它 RGB-like 模板点型保持标量，下一步要 new phase |
| staging / reduction | adopted | quantize 直写、filter byte histogram、无额外 staging buffer | `extractFeatures()` 和 `computeDistanceMap()` 未改 |
| formula / FMA | not_now | 当前是整数缩放比较和 byte 计数，不需要单独 FMA 审计 | 如要改成浮点近似或更复杂公式，需新 phase |
| production scope | adopted | public entry + board repeated + asm + doctor 全闭合 | 当前仅 `PointXYZRGB` public overload |

## 生产接入判断

`production_patch_scope`：`recognition/include/pcl/recognition/color_modality.h`，新增 `quantizeColorsRVV()`、`filterQuantizedColorsRVV()` 和 `processInputData()` 的 RVV 分流。

`covered_path`：organized `PointXYZRGB`、`width >= 3 && height >= 3`、public `processInputData()`、RGB extrema quantize、3x3 filter、spread。

`fallback_matrix`：非 RVV 构建、强制标量 hook、小图、非 exact `PointXYZRGB`、`extractFeatures()`、`computeDistanceMap()` 保持标量。

`production_direct_tests`：`run_test_compare` 通过，`run_bench_rvv` QEMU smoke 可运行，`board_repeated` 5-run 正向。

`production_asm`：`check_cm_rvv_asm` 通过，RVV 指令可归属到 bench RVV helper / inlined path。

`production_board_bench`：Phase 040 repeated board 正向，`2.490x` / `2.410x`。

`decision_delta`：Phase 030 只证明 filter-only family；Phase 040 证明 quantize+filter family 在同一 public entry boundary 内更快，因此当前采用 Phase 040 family。

## Traceability Map

| 符号 / 文件 | 层级 | 作用 | 调用者 / 上游入口 | 被调用者 / 下游消费者 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- | --- | --- |
| `ColorModality<PointXYZRGB>::processInputData` | production public entry | 生产预处理入口 | recognition caller | RVV / Std helper、spread | production boundary | `recognition/include/pcl/recognition/color_modality.h` |
| `processInputDataStd` | production Std helper | 标量事实来源 | public fallback | quantize / filter / spread | fallback source of truth | 同上 |
| `quantizeColorsRVV` | production RVV helper | RGB extrema quantize RVV | public dispatch | `quantized_colors_` | adopted production RVV path | 同上 |
| `filterQuantizedColorsRVV` | production RVV helper | 3x3 dominant filter RVV | public dispatch | `filtered_quantized_colors_` | adopted production RVV path | 同上 |
| `test_cm.cpp` | correctness target | forced scalar/RVV 对拍和 public entry 路径命中 | `run_test_compare` | gtest assertions | correctness gate | `test-rvv/recognition/color_modality/src/test_cm.cpp` |
| `bench_cm.cpp` | bench wrapper | production direct / diagnostic bench | board / QEMU bench target | summary / manifest | performance input | `test-rvv/recognition/color_modality/src/bench_cm.cpp` |
| `generate_cm_evidence_manifest.py` | analysis script | summary / manifest 生成 | `record_evidence_state_repeated` | evidence registry | evidence metadata | `test-rvv/recognition/color_modality/script/generate_cm_evidence_manifest.py` |
| Phase 040 result | phase result | 生产接入闭环和边界审计 | phase doc | evaluation / doc-rvv | adoption audit | `test-rvv/recognition/color_modality/doc/phases/040-rgb-extrema-quantize-production-rvv/result.zh.md` |
| repeated board summary | evidence output summary | board performance 摘要 | board repeated target | evaluation / doc-rvv | board performance | `test-rvv/recognition/color_modality/log/board/repeated_phase040_quantize_filter_rvv/summary.md` |

## 增量诊断结果

没有新的继续优化方向可以在当前授权范围内闭合。`extractFeatures()` / `computeDistanceMap()` 需要先通过 profile 或 component ablation 证明是热点，再决定是否另开 phase；generic RGB traits 扩展则需要新的调用需求和独立 fallback / board 证据。
