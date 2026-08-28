# ColorGradientModality RVV 生产接入说明

## 当前状态

`recognition/include/pcl/recognition/color_gradient_modality.h` 当前采用
`rgb-stencil-production-rvv`。该路径只接管
`ColorGradientModality<PointInT>::processInputData()` 中 Gaussian convolution（高斯卷积）
之后、`QuantizedMap::spreadQuantizedMap()` 之前的 color-gradient 链路；public API
（公开接口）、Gaussian、spread 和 feature extraction（特征提取）保持原有形态。

采纳依据是 Phase 050 production direct（真实生产路径）板卡重复测试。Std/RVV 两侧都通过
`ColorGradientModality<PointXYZRGB>::processInputData()` 公开入口计时，case 覆盖常规宽度和
RVV tail lanes（尾部向量 lane）：

| case | 入口 | runs | median speedup | range | B/A < 1 | checksum |
| --- | --- | ---: | ---: | --- | ---: | --- |
| `production_process_320x240` | `ColorGradientModality<PointXYZRGB>::processInputData()` | 5 | `1.620x` | `1.610x` - `1.680x` | `0/5` | Std/RVV 一致：`5189351474172833280` |
| `production_process_641x481_tail` | `ColorGradientModality<PointXYZRGB>::processInputData()` | 5 | `1.590x` | `1.580x` - `1.660x` | `0/5` | Std/RVV 一致：`2369217414544299322` |

证据路径：

- production direct summary:
  `test-rvv/recognition/color_gradient_modality/log/board/repeated_phase050_rgb_stencil_production_direct/summary.md`
- Evidence Doctor（证据体检）:
  `test-rvv/recognition/color_gradient_modality/log/board/repeated_phase050_rgb_stencil_production_direct/evidence_doctor.md`
- Phase 050 result:
  `test-rvv/recognition/color_gradient_modality/doc/phases/050-rgb-stencil-production-integration/result.zh.md`
- 函数级评估:
  `test-rvv/recognition/color_gradient_modality/doc/color_gradient_modality-evaluation.zh.md`

Phase 030 的 `post-gaussian-production-rvv` 是历史已采纳基线，板卡 median 为
`1.490x` / `1.540x`。Phase 050 在同一 public entry 重新接入并测试后仍为 positive，
因此按用户确认的“接入后板卡有收益即可采纳”策略替换为当前生产实现。QEMU（仿真器）只用于
正确性、构建和日志形状；性能结论只来自板卡重复测试。

## 函数语义和标量路径

`processInputData()` 处理 organized RGB 点云。它先把模板输入点类型中的 `r/g/b`
字段复制到 `pcl::RGB` 云，再用固定大小的 Gaussian kernel 生成 `smoothed_input_`。
标量 color-gradient 链路随后执行三步：

1. `computeMaxColorGradientsSobel()` 对内区像素计算 RGB 三通道 3x3 Sobel 梯度，并选择
   squared magnitude（平方幅值）最大的通道。
2. `quantizeColorGradients()` 对最大通道梯度计算 magnitude（幅值）和 angle（角度），再按
   `gradient_magnitude_threshold_` 与 8 个方向 bin（方向桶）生成 `quantized_color_gradients_`。
3. `filterQuantizedColorGradients()` 在 3x3 邻域中寻找至少出现 5 次的 dominant bin
   （主方向桶），写出 one-hot 形式的 `filtered_quantized_color_gradients_`。

最后 `QuantizedMap::spreadQuantizedMap()` 把 filtered map 扩展成
`spreaded_filtered_quantized_color_gradients_`，供 `extractFeatures()` 使用。

## 当前采用的优化方式

| 维度 | 当前状态 | 采用或暂缓原因 | 证据 | 边界 / 下一步 |
| --- | --- | --- | --- | --- |
| production dispatch（生产分流） | adopted | `processInputData()` 在 `__RVV10__` 下先尝试 RVV helper，失败时自然回退 Std helper | `run_test_compare` 和 production path hook | 非 RVV 构建不编译 RVV helper |
| 标量 fallback（回退路径） | adopted | 原 Sobel、quantize、filter 被抽为 `computeColorGradientPipelineStd()`，保留事实来源 | forced scalar/RVV 同进程对拍 | 小图、非 RVV 构建和测试强制标量都回到 Std |
| RGB Sobel stencil | adopted | RVV 直接从 `pcl::RGB` B/G/R/A 四字节布局做 `vlse8` 跨步读取、widen（拓宽）和三通道 Sobel，避免整图 `selected_dx/selected_dy/selected_sqr_mag` staging | Phase 040 diagnostic positive；Phase 050 production direct positive | 只接 `pcl::RGB` Gaussian 后内部 buffer，不直接读取模板 `PointInT` RGB 字段 |
| RVV 数学链路 | adopted | RVV 接管 `sqrt`、`atan2_RVV_f32m2`、角度归一化、方向量化和阈值 mask | production direct correctness、asm、board | `GradientXY::angle` 是容差一致，不是 bit-exact libm |
| 3x3 dominant filter | adopted | RVV 按方向逐 bin 计数，严格 `>` 保持标量 tie-break（并列处理） | Phase 010、Phase 050、asm | 边框仍保持标量语义 |
| feature extraction | not_now | 当前 production_process 计时边界不包含 feature extraction，且 list/sort/distance 选择会引入状态和顺序风险 | evaluation / roadmap | 若继续榨取完整模板生成收益，应先做 `extractFeatures()` profile 或 component ablation（组件消融） |

RVV helper 的基本流程是：

```cpp
// production gate: width/height >= 3, __RVV10__ enabled, no forced scalar test hook.
// RVV Sobel stencil: stride-load B/G/R bytes from smoothed pcl::RGB and widen to int32.
// RVV channel select: match scalar red/green/blue tie-break on squared magnitude.
// RVV math chunk: compute sqrt and atan2, normalize angle, quantize directions.
// scalar state bridge: write GradientXY and quantized map entries for downstream production state.
// RVV filter chunk: load 3x3 quantized bytes, count dominant bins, store one-hot filtered map.
```

这不是泛型 RGB 字段直接向量加载方案。当前采用方式保守地避开 `PointInT` 字段 traits
（点类型字段特征）和布局扩展：production RVV 只处理已经由公开入口复制并平滑后的
`pcl::RGB` organized buffer。

## 覆盖范围与 fallback

| 范围 | 当前状态 | 证据 / 原因 |
| --- | --- | --- |
| `PointXYZRGB` organized public input | adopted | production direct bench 和 gtest 使用该公开入口 |
| `pcl::RGB` Gaussian 后内部 buffer | adopted | RVV helper 输入为 `smoothed_input_`，stride 依据 `sizeof(pcl::RGB)` 和字段 offset |
| `width >= 3 && height >= 3` | adopted | RVV helper gate 覆盖内区像素，tail case 已测 |
| `width < 3 || height < 3` | scalar fallback | RVV helper 返回 false，`processInputData()` 调用 Std |
| 非 RVV 构建 | scalar fallback | `__RVV10__` 未启用时只编译 Std 路径 |
| 其它可编译 `PointInT` 模板实例 | scalar-compatible public API | RGB 复制仍由模板源码负责；当前性能证据只批准 `PointXYZRGB` public case |
| indices / correspondences | not_applicable with evidence | `ColorGradientModality::processInputData()` 本身不提供 indices / correspondences 分支 |
| feature extraction | not_now | 当前 RVV patch 不修改 `extractFeatures()` |

## 正确性与高效性证据链

| 证据层 | 当前结果 | 能证明什么 | 不能证明什么 |
| --- | --- | --- | --- |
| correctness（正确性） | `make -C test-rvv/recognition/color_gradient_modality run_test_compare` 通过，Std/RVV 均 8/8 | public entry 命中、forced scalar/RVV 对拍、map 和 feature 输出一致 | 不能证明目标板卡性能 |
| QEMU smoke | production case 极小迭代可运行 | 日志形状和入口可运行 | QEMU timing 不作性能结论 |
| asm（反汇编） | `check_cgm_production_rvv_asm` 和 `check_cgm_stencil_rvv_asm` 通过 | production helper 命中 `vlse8`、widen/integer Sobel、`vfsqrt`、convert、byte compare/merge/store 和 `atan2_RVV` 相关指令 | 不能单独证明端到端收益 |
| board repeated（板卡重复测试） | median `1.620x` / `1.590x`，checksum 一致，`B/A < 1` 为 `0/5` | 当前 public RVV path 快于当前 public scalar path | 不单独覆盖 `extractFeatures()` 计时 |
| Evidence Doctor | `Errors=0 / Warnings=0 / Suggestions=4` | 当前 summary 无阻塞错误或警告 | 环境字段和 binary hash 仍可补强 |

`atan2_RVV_f32m2` 是近似 helper，不是 strict libm replacement（严格 libm 替换）。
因此 bench checksum 只严格覆盖 production 后续直接消费的 quantized map 和 spreaded map；
`GradientXY::angle` 由 gtest 容差门禁覆盖，并且 `extractFeatures()` 输出已对拍。

## Bench case 说明

| case | 数据 | 计时边界 | 证明点 |
| --- | --- | --- | --- |
| `production_process_320x240` | synthetic organized `PointXYZRGB`，320x240 | RGB copy、Gaussian、Gaussian 后 color-gradient Std/RVV 链路、spread | 常规尺寸 public entry 生产收益 |
| `production_process_641x481_tail` | synthetic organized `PointXYZRGB`，641x481 | 同上，且覆盖 RVV tail lanes | 非整齐宽度下仍稳定正向 |

Phase 000-020 的 Sobel、filter 和 full-chain case 是 production-shaped diagnostic
（生产形态诊断），只作为候选筛选依据；正式性能结论以上表 production direct case 为准。

## Traceability Map

| 符号 / 文件 | 层级 | 作用 | 调用者 / 上游入口 | 被调用者 / 下游消费者 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- | --- | --- |
| `ColorGradientModality::processInputData` | production public entry | 完整 color-gradient 预处理入口 | LINEMOD modality 使用方 | Gaussian、color-gradient pipeline、spread | production boundary | `recognition/include/pcl/recognition/color_gradient_modality.h` |
| `computeColorGradientPipelineStd` | production Std helper | 原 Sobel+quantize+filter 标量事实来源 | `processInputData()` fallback | `spreadQuantizedMap()` | fallback source of truth | `recognition/include/pcl/recognition/color_gradient_modality.h` |
| `computeColorGradientPipelineRVV` | production RVV helper | Gaussian 后 RVV color-gradient pipeline | `processInputData()` RVV dispatch | `spreadQuantizedMap()` | adopted production RVV path | `recognition/include/pcl/recognition/color_gradient_modality.h` |
| `test_cgm.cpp` | correctness target | path-hit、forced scalar/RVV 对拍、feature 输出一致 | `run_test_compare` | gtest assertions | correctness gate | `test-rvv/recognition/color_gradient_modality/src/test_cgm.cpp` |
| `bench_cgm.cpp` | bench wrapper | production direct case 和 diagnostic case | board/QEMU bench target | summary / manifest | performance input | `test-rvv/recognition/color_gradient_modality/src/bench_cgm.cpp` |
| `generate_cgm_evidence_manifest.py` | analysis script | 生成 board summary 和 Evidence Doctor manifest | `record_evidence_state_repeated` | evidence registry | evidence metadata | `test-rvv/recognition/color_gradient_modality/script/generate_cgm_evidence_manifest.py` |
| Phase 050 summary | evidence output summary | 当前 production direct 板卡摘要 | board repeated target | evaluation / 本文 | board performance | `test-rvv/recognition/color_gradient_modality/log/board/repeated_phase050_rgb_stencil_production_direct/summary.md` |
| Phase 050 result | phase result | RGB stencil 生产接入闭环审计 | PI2-PI5 | evaluation / 本文 | adoption audit | `test-rvv/recognition/color_gradient_modality/doc/phases/050-rgb-stencil-production-integration/result.zh.md` |

## Production closeout

| 项 | 最终状态 | 证据 |
| --- | --- | --- |
| production patch | adopted | 用户确认“板卡有收益即可采纳”；Phase 050 public entry board positive |
| public API | unchanged | 只新增内部 helper 和 `__RVV10__` guarded dispatch |
| fallback | adopted | 非 RVV 构建、小图、强制标量 hook 均可回到 Std |
| 文档归属 | adopted | 本文保存长期 production 行为；topic-local evaluation / phase docs 保存候选与阶段审计 |
| 后续优化 | deferred | `feature-prefilter-audit` 需先用 profile / ablation 证明 `extractFeatures()` 仍值得 RVV 化 |

## 遗留风险与后续条件

- `GradientXY::angle` 不是 bit-exact libm 输出；需要严格角度 getter 一致性时，应另开
  strict-libm / angle-compat phase。
- Phase 050 是 public RVV path 对 public scalar path 的生产性能证据。它满足本轮采纳策略；
  若后续要证明 Phase 050 相对 Phase 030 的微小增益，应补同一 production boundary 内的
  RVV-vs-RVV detail A/B。
- `extractFeatures()` 当前仍保持标量。若后续 profile 指出 feature extraction 主导完整模板生成，
  应先做 component ablation，再考虑 mask prefilter 或保序 candidate list 优化。
