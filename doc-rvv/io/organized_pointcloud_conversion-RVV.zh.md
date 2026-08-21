# organized_pointcloud_conversion RVV

## 当前状态

`io/include/pcl/compression/organized_pointcloud_conversion.h` 的 cloud encode overload 和 `OrganizedPointCloudCompression<PointT>::analyzeOrganizedCloud` 的 protected detail helper 已采用 RVV production path（生产路径）。当前接入范围是：

- `OrganizedConversion<PointT, false>::convert(cloud, ...)`：点云转 disparity image（视差图）。
- `OrganizedConversion<PointT, true>::convert(cloud, ...)`：彩色点云转 disparity + RGB / mono buffer。
- `OrganizedPointCloudCompression<PointT>::analyzeOrganizedCloud`：扫描 organized cloud 得到 max depth 和 focal length。

不覆盖范围同样是生产事实的一部分：disparity/depth image -> cloud 的 decode overload 保持标量；`OrganizedPointCloudCompression::encodePointCloud` 真实 public class entry 尚未被 no-OpenNI cross build 直接实例化。当前已有 encode-shaped helper 的 production-shaped diagnostic 显示接入 analyze detail 后完整形态链路约 1.12x-1.17x 正向，但它不是 public class direct evidence。

## 函数语义和调用链

`OrganizedPointCloudCompression::encodePointCloud` 先调用 `analyzeOrganizedCloud` 得到 `maxDepth` 和 `focalLength`，再调用 `OrganizedConversion<PointT>::convert` 把 organized cloud 转成 disparity buffer 和可选 color buffer，最后把这些 buffer 交给 PNG 编码并写入 stream。

被 RVV 接管的是两段生产逻辑：一是中间 conversion 阶段，它对每个点读取 `x/y/z`，若 `pcl::isFinite(point)` 为真，则计算：

```text
disparity = uint16_t(focalLength / (disparityScale * z) + disparityShift / disparityScale)
```

非 finite 点输出 disparity 0。Colored path 在同一 finite 判断下写出 RGB 三字节，或按 `0.2989*r + 0.5870*g + 0.1140*b` 写 mono 单字节；invalid 点颜色输出 0。

二是 `OrganizedPointCloudCompression<PointT>::analyzeOrganizedCloud` 调用的 production-detail helper：RVV path 分块读取 x/y/z，找出最大 finite z 对应的点，再用原公式计算 focal length。该 helper 是 protected detail boundary（受保护的内部生产边界）；它已服务真实 production 调用链，但当前证据仍不等同于完整 public class direct。

## 当前采用的优化方式

| 维度 | 当前状态 | 采用或暂缓原因 | 证据 | 边界 / 下一步 |
| --- | --- | --- | --- | --- |
| uncolored cloud encode | adopted | x/y/z AoS stride load、finite mask 和 disparity 公式适合 RVV 分块 | production direct `PointXYZ` / `PointXYZI` positive | 更多自定义点型需扩展 phase |
| colored cloud encode | adopted | 复用同一次 RVV finite/disparity pass，color 写回保留逐 lane 标量以守住语义 | production direct `PointXYZRGB` / `PointXYZRGBA` positive | RGB/mono 分开报告 |
| decode disparity/depth -> cloud | rejected | v0 per-VL x/y staging 和 AoS 写回成本高于公式收益 | diagnostic board 0.85x-0.91x，shared Doctor decode Errors | 只有新写回方案出现时恢复 |
| full `encodePointCloud` shaped compression | attempted_positive | production-shaped helper 仍有约 1.15x 正向收益 | `log/board/full_encode_repeated/summary.md`，Doctor clean | 不是真实 public class direct evidence |
| `analyzeOrganizedCloud` RVV | adopted production-detail | detail helper 1.820x-3.908x，after-patch full shaped 仍正向 | `log/board/analyze_production_detail_repeated/summary.md`、`log/board/full_encode_after_analyze_detail_repeated/summary.md` | 不是真实 public class direct |

RVV helper 使用 `pcl::rvv::kRVVXYZAoSPointCompatible<PointT>` gate（字段特征和 AoS 布局准入）证明当前点类型的 `x/y/z` 是单个 `float` 且可按当前 `PointT` stride 读取。进入 RVV 后，代码按 VL chunk（可变向量长度分块）读取 x/y/z，使用 `vfclass` 生成 finite 判断，把 disparity 公式批量计算到 per-VL scratch，再逐 lane 写回 `std::vector` 输出。

Colored path 不向量化 RGB 字节 pack。这样做保留 `PointXYZRGB` / `PointXYZRGBA` 字段读取语义，避免在 production 中新增未验证的 packed color load/store helper；板卡证据显示即便保留逐 lane color 写回，RGB path 仍稳定正向。

Analyze helper 也使用同一 xyz AoS traits gate。RVV path 只批量完成 finite 分类和最大 z 搜索；focal length 仍在最大深度点确定后按 scalar 公式计算一次，避免改变 max-depth tie-break（相同最大深度选择）和除零边界语义。

## Fallback 矩阵

| 条件 | 行为 | 语义保持证据 |
| --- | --- | --- |
| 非 RVV 构建或未定义 `__RVV10__` | 只编译 Std helper | Std build correctness |
| 点类型不满足 xyz 单 float AoS gate | RVV helper 返回 false，公开 overload 调 Std helper | compile-time traits gate |
| cloud size < 64 | 回退 Std helper | small-size gate 避免短输入维护成本 |
| analyze helper 不满足 RVV gate | 回退 `analyzeOrganizedCloudStd` | detail helper correctness 对拍 |
| decode overload | 保持原标量循环 | `run_test_compare` 覆盖 decode v0 对拍；生产未接 RVV |
| invalid / NaN / Inf 点 | RVV path 输出 0，与 `pcl::isFinite` 标量语义一致 | literal 和 mixed-invalid gtest |
| custom xyz-like / normal 复合点型 | traits 可能命中，但 performance 未声明 | 后续 point-type expansion 需要独立证据 |

## 范围决策表

| 范围 | 状态 | 当前证据 | 不能外推到哪里 |
| --- | --- | --- | --- |
| `PointXYZ` cloud -> disparity | adopted | production direct 5-run median 约 2.11x | full compression chain |
| `PointXYZI` cloud -> disparity | adopted representative | production direct 5-run median 约 2.01x-2.05x | all custom xyz-like layouts |
| `PointXYZRGB` cloud -> RGB disparity | adopted | production direct RGB median 约 1.37x-1.38x | mono 或 RGBA exact behavior |
| `PointXYZRGB` cloud -> mono disparity | adopted | production direct mono median 约 1.69x，Doctor group_outlier warning explained | RGB path |
| `PointXYZRGBA` cloud -> RGB disparity | adopted representative | production direct median 约 1.36x-1.41x | arbitrary color point types |
| disparity/depth decode | scalar-only / rejected RVV v0 | diagnostic 0.85x-0.91x negative | production adoption |
| encodePointCloud-shaped helper | attempted_positive | production-shaped board median 1.147x-1.157x | real public class entry |
| `analyzeOrganizedCloud` production detail | adopted | production-detail board median 3.908x / 3.815x / 1.820x；after-patch full shaped 1.119x / 1.147x / 1.174x | real public class direct |

## Traceability Map

| 符号 / 文件 | 层级 | 作用 | 调用者 / 上游入口 | 被调用者 / 下游消费者 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- | --- | --- |
| `OrganizedConversion<PointT,false>::convert(cloud, ...)` | production public entry | uncolored cloud encode dispatch | `encodePointCloud` / public callers | Std 或 RVV helper | production boundary | `io/include/pcl/compression/organized_pointcloud_conversion.h` |
| `OrganizedConversion<PointT,true>::convert(cloud, ...)` | production public entry | colored cloud encode dispatch | `encodePointCloud` / public callers | Std 或 RVV helper | production boundary | `io/include/pcl/compression/organized_pointcloud_conversion.h` |
| `convertCloudToDisparityStd` | production Std helper | uncolored scalar fallback | public overload | disparity vector | baseline | `io/include/pcl/compression/organized_pointcloud_conversion.h` |
| `convertCloudToDisparityColorStd` | production Std helper | colored scalar fallback | public overload | disparity + color vectors | baseline | `io/include/pcl/compression/organized_pointcloud_conversion.h` |
| `convertCloudToDisparityRVV` | production RVV helper | uncolored RVV conversion | public overload | disparity vector | adopted RVV path | `io/include/pcl/compression/organized_pointcloud_conversion.h` |
| `convertCloudToDisparityColorRVV` | production RVV helper | colored RVV conversion | public overload | disparity + color vectors | adopted RVV path | `io/include/pcl/compression/organized_pointcloud_conversion.h` |
| `organized_compression_detail::analyzeOrganizedCloud` | production detail dispatch | max-depth / focal-length Std/RVV 分流 | `OrganizedPointCloudCompression::analyzeOrganizedCloud` | max depth + focal length | adopted RVV detail path | `io/include/pcl/compression/impl/organized_pointcloud_compression_analysis.hpp` |
| `OrganizedPointCloudCompression::analyzeOrganizedCloud` | protected production helper | 委托到 detail dispatch | `encodePointCloud` | max depth + focal length | production detail boundary | `io/include/pcl/compression/impl/organized_pointcloud_compression.hpp` |
| `test_organized_pointcloud_conversion.cpp` | correctness gate | Std/RVV semantic checks | `make run_test_compare` | gtest output | correctness evidence | `test-rvv/io/organized_pointcloud_conversion/src/test_organized_pointcloud_conversion.cpp` |
| `bench_organized_pointcloud_conversion.cpp` | bench wrapper | diagnostic and production direct timing | board runner | summary / manifest | board performance evidence | `test-rvv/io/organized_pointcloud_conversion/src/bench_organized_pointcloud_conversion.cpp` |
| `generate_opc_board_evidence_manifest.py` | analysis script | topic logs -> Evidence Doctor manifest | Make targets | JSON manifest | doctor input | `test-rvv/io/organized_pointcloud_conversion/script/generate_opc_board_evidence_manifest.py` |
| production repeated summary | evidence output summary | 5-run production direct results | board logs | Evidence Doctor | performance evidence | `test-rvv/io/organized_pointcloud_conversion/log/board/production_direct_repeated/summary.md` |
| full encode repeated summary | evidence output summary | encode-shaped 5-run results | board logs | Evidence Doctor | production-shaped evidence | `test-rvv/io/organized_pointcloud_conversion/log/board/full_encode_repeated/summary.md` |
| analyze component repeated summary | evidence output summary | analyze diagnostic 5-run results | board logs | Evidence Doctor | component diagnostic | `test-rvv/io/organized_pointcloud_conversion/log/board/analyze_component_repeated/summary.md` |
| analyze production-detail repeated summary | evidence output summary | analyze detail 5-run results | board logs | Evidence Doctor | production-detail performance | `test-rvv/io/organized_pointcloud_conversion/log/board/analyze_production_detail_repeated/summary.md` |
| after-patch full encode repeated summary | evidence output summary | analyze detail 接入后的 encode-shaped 5-run results | board logs | Evidence Doctor | production-shaped context | `test-rvv/io/organized_pointcloud_conversion/log/board/full_encode_after_analyze_detail_repeated/summary.md` |
| topic evaluation | documentation section | adoption decision and remaining risks | worker / reviewer | this doc and phase docs | decision audit | `test-rvv/io/organized_pointcloud_conversion/doc/organized_pointcloud_conversion-evaluation.zh.md` |

## VL chunk 算例

以 `focalLength=525`、`disparityScale=0.5`、`disparityShift=2` 为例：

| lane | x | y | z | finite | RVV formula result | output |
| --- | ---: | ---: | ---: | --- | ---: | ---: |
| 0 | 0 | 0 | 1 | true | `525/(0.5*1)+4 = 1054` | 1054 |
| 1 | 1 | -1 | 2 | true | `525/(0.5*2)+4 = 529` | 529 |
| 2 | NaN | 2 | 3 | false | computed value ignored | 0 |
| 3 | 3 | 4 | Inf | false | computed value ignored | 0 |

RVV path 会对所有 lane 批量计算公式，但只有 `vfclass(x/y/z)` 均判定 finite 的 lane 才写回结果。这个行为与标量路径“先 `pcl::isFinite(point)`，再 push disparity，否则 push 0”一致。

## Bench 与证据

Production direct 证据只比较 public overload 的 Std build 和 RVV build，B/A 方向为 `Std ms / RVV ms`，大于 1 表示 RVV 更快。当前 5-run repeated summary：

| case | min | median | max |
| --- | ---: | ---: | ---: |
| `production_pointxyz_disparity_dense_307k` | 2.107x | 2.109x | 2.113x |
| `production_pointxyz_disparity_mixed_invalid_307k` | 2.070x | 2.071x | 2.073x |
| `production_pointxyzi_disparity_dense_307k` | 2.045x | 2.048x | 2.052x |
| `production_pointxyzi_disparity_mixed_invalid_307k` | 2.009x | 2.012x | 2.016x |
| `production_pointxyzrgb_disparity_rgb_dense_307k` | 1.367x | 1.370x | 1.378x |
| `production_pointxyzrgb_disparity_mono_dense_307k` | 1.637x | 1.690x | 1.765x |
| `production_pointxyzrgb_disparity_rgb_mixed_invalid_307k` | 1.357x | 1.382x | 1.387x |
| `production_pointxyzrgba_disparity_rgb_dense_307k` | 1.395x | 1.412x | 1.427x |
| `production_pointxyzrgba_disparity_rgb_mixed_invalid_307k` | 1.346x | 1.362x | 1.382x |

Evidence Doctor: `Errors=0, Warnings=1, Suggestions=0`。Warning 是 mono case 的 group outlier；处理策略是按 RGB / mono 分开报告，不把 mono 收益外推到 RGB。

后续 production-shaped 与 analyze 证据：

| evidence | min | median | max | Doctor |
| --- | ---: | ---: | ---: | --- |
| `production_full_pointxyz_encode_dense_307k` | 1.146x | 1.147x | 1.153x | Errors=0, Warnings=0 |
| `production_full_pointxyzrgb_encode_rgb_dense_307k` | 1.142x | 1.148x | 1.154x | Errors=0, Warnings=0 |
| `production_full_pointxyzrgb_encode_mono_dense_307k` | 1.156x | 1.157x | 1.173x | Errors=0, Warnings=0 |
| `analyze_pointxyz_organized_dense_307k` | 3.512x | 3.548x | 3.579x | Errors=0, Warnings=0 |
| `analyze_pointxyz_organized_mixed_invalid_307k` | 3.410x | 3.483x | 3.496x | Errors=0, Warnings=0 |
| `production_analyze_detail_pointxyz_dense_307k` | 3.783x | 3.908x | 3.947x | Errors=0, Warnings=1 |
| `production_analyze_detail_pointxyz_mixed_invalid_307k` | 3.739x | 3.815x | 3.846x | Errors=0, Warnings=1 |
| `production_analyze_detail_pointxyzi_mixed_invalid_307k` | 1.798x | 1.820x | 1.840x | Errors=0, Warnings=1 |
| `production_full_pointxyz_encode_dense_307k` after analyze detail | 1.096x | 1.119x | 1.140x | Errors=0, Warnings=0 |
| `production_full_pointxyzrgb_encode_rgb_dense_307k` after analyze detail | 1.137x | 1.147x | 1.165x | Errors=0, Warnings=0 |
| `production_full_pointxyzrgb_encode_mono_dense_307k` after analyze detail | 1.168x | 1.174x | 1.196x | Errors=0, Warnings=0 |

`production_analyze_detail_*` 是当前采用 analyze helper 的 production-detail 证据。`production_full_* after analyze detail` 是 encodePointCloud-shaped helper，不是真实 public class entry；它只说明 analyze detail 接入后，上下文形态仍正向。

## 正确性与高效性证据链

| evidence area | 当前证据 | 结论边界 |
| --- | --- | --- |
| correctness | `make run_test_compare`，Std/RVV 各 14 个 TEST pass | 证明输出语义一致，不证明性能 |
| path / asm | `make check_production_rvv_asm` pass | 证明 conversion 和 analyze detail production probe 中存在 RVV 指令，不单独证明收益 |
| board performance | production direct repeated 9 cases 全部 positive | 只覆盖 conversion public overload |
| production-shaped performance | full encode-shaped repeated 3 cases positive | 不替代真实 public class evidence |
| production-detail performance | analyze detail repeated 3 cases positive | 支持 protected detail helper adoption，不替代 public class direct |
| Evidence Doctor | conversion production repeated `Errors=0, Warnings=1`；analyze detail repeated `Errors=0, Warnings=1`；after-patch shaped clean | warnings 已解释；decode shared Errors 不参与 adoption |
| fallback | non-RVV、unsupported layout、小规模和 decode 保持 Std | 不覆盖未验证 custom point types 的性能 |

## Production closeout

| 项 | 最终状态 | 证据 |
| --- | --- | --- |
| production files | 修改 `io/include/pcl/compression/organized_pointcloud_conversion.h`，新增 `io/include/pcl/compression/impl/organized_pointcloud_compression_analysis.hpp`，并让 `organized_pointcloud_compression.hpp` 委托 analyze detail helper | source diff |
| public API | 不改变 public API、参数或返回类型 | header shape |
| compile gate | `__RVV10__` 下编译 RVV helper，非 RVV 构建自然走 Std | build / test |
| point type gate | `kRVVXYZAoSPointCompatible<PointT>` + representative board evidence | generic strategy + production repeated |
| decode | 保持标量 | Phase 030 rejected |
| rollback boundary | 可通过移除 RVV helper include / dispatch 回到 Std helper | production file single-topic diff |

## 后续方向

当前没有必须继续推进的高优先级性能候选。若要把 shaped helper 证据升级为真实 public class direct，需要先解决 no-OpenNI cross build 无法实例化 `OrganizedPointCloudCompression` 的问题。泛型点型扩展可以另开 phase：当前 production gate 是 traits-based，但已验证性能只覆盖代表点型。自定义点型、normal 复合点型或更多 color layouts 需要 dedicated correctness、asm、board repeated 和 Evidence Doctor。Color byte vector pack 只有在 profile 或同边界 A/B 证明逐 lane color 写回仍是主要瓶颈时才值得重开。
