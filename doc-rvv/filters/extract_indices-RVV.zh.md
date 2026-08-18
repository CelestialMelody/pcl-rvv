# filters/extract_indices RVV 诊断说明

## 1. 函数入口作用

`pcl::ExtractIndices<PointT>` 是 filters 模块中按索引集合提取或剔除点的通用滤波器。用户设置 `input_` 和 `indices_` 后调用 `filter(output)` 或 `filter(indices)`：

- `negative=false`：保留 `indices_` 指向的点；
- `negative=true`：保留全集中不在 `indices_` 内的点；
- `keep_organized=true` 或 `filterDirectly`：不改变点云尺寸，而是把被剔除点的字段写为 `user_filter_value_`；
- `extract_removed_indices=true`：同时记录被移除的索引。

该入口在算法管线中承担“按已有索引结果执行提取 / 剔除 / 坏点写回”的后处理职责。它自身不判断几何或字段 predicate，主成本通常来自补集生成、整点复制、字段循环写回和内存流量。

本主题来自保留候选复筛的 diagnostic / bench-only 路径记录。当前不修改 `filters/include/pcl/filters/impl/extract_indices.hpp` 或 `filters/src/extract_indices.cpp` 的生产分流，只在 `test-rvv/filters/extract_indices/` 中保留诊断 helper、测试和 bench。

## 2. 上游标量路径与诊断边界

`ExtractIndices<PointT>::applyFilterIndices` 的补集路径为：

```text
full_indices = [0, 1, ..., input_size-1]
sorted_input_indices = sort(indices_)
complement = set_difference(full_indices, sorted_input_indices)
```

`applyFilter(PointCloud&)` 在 `keep_organized=false` 时继续 `copyPointCloud(*input_, indices, output)`；在 `keep_organized=true` 时先复制整云，再把 removed 位置所有字段写为 `user_filter_value_`。`filterDirectly` 也会按 `FieldList` 对所有字段执行原地写坏点。

当前诊断拆成三个层次：

| 层次                   | 对应 bench case                                 | 说明                                                                                  |
| ---------------------- | ----------------------------------------------- | ------------------------------------------------------------------------------------- |
| complement bitmap 片段 | `extract indices complement diag ...`         | scalar 构造 membership bitmap，RVV 扫描 bitmap 中为 0 的 lane 并输出补集 indices      |
| keep organized 诊断    | `extract indices keep organized ... diag ...` | 在 `PointXYZ` 上执行补集或 selected sparse 写坏点，只覆盖 `x/y/z` 三个 float 字段 |
| production unchanged   | `extract indices production unchanged ...`    | 未修改上游生产入口，仅观察当前整体入口成本和确认分流没有改变                          |

局部片段收益不能直接作为生产接入依据。生产路径还需要处理无序 / 重复 indices、泛型字段列表、`PCLPointCloud2` 字节布局、`copyPointCloud`、全云复制和 `removed_indices_` 语义。

## 3. 覆盖范围与 fallback

| 项目       | 结论                                                                                               |
| ---------- | -------------------------------------------------------------------------------------------------- |
| 覆盖点类型 | 诊断固定 `pcl::PointXYZ`                                                                         |
| 覆盖输入   | 全云大小和 selected indices 均需可由 32 位 lane 表达                                               |
| RVV 内容   | bitmap absent mask +`vcompress` 输出补集；selected / complement 的 `x/y/z` scatter 写坏点      |
| 保持标量   | bitmap 构造、生产 `ExtractIndices`、`copyPointCloud`、泛型字段写回、`PCLPointCloud2`、小规模 |
| 公开 API   | 不改变                                                                                             |
| FRM/FCSR   | 不涉及显式舍入模式，不修改浮点环境                                                                 |

## 4. 详细设计

专项实现位于 `test-rvv/filters/extract_indices/extract_indices_diag.hpp`。

本主题的 RVV 诊断路径与上游标量源码形态明显不同：标量源码显式执行 `sort(indices_)` 后调用 `set_difference`，而诊断 RVV 没有实现任何排序算法。这里采用的是针对“连续全集 `[0,n)` 求补集”的算法等价改写：

```text
标量形式：
  full_indices = [0, 1, ..., n-1]
  sorted = sort(indices_)
  complement = set_difference(full_indices, sorted)

诊断形式：
  bitmap[indices_[k]] = 1        # 标量阶段，处理无序 / 重复 selected
  for i in [0,n):                # RVV 阶段，线性扫描全集
    if bitmap[i] == 0:
      complement.push_back(i)
```

这个改写成立的关键是：`full_indices` 本身就是连续递增整数，补集输出也要求按递增 source index 输出。bitmap 标记后，RVV 按 `i=0..n-1` 单调扫描，用 `vid + chunk_base` 生成同样递增的 source index，再用 `vcompress` 保序输出 absent lane。因此输出顺序与 `set_difference` 在有序全集上的输出一致。

需要特别区分：

- 这里不是 `RVV sort(indices_)`，也没有 bitonic sort、merge sort 或其它向量排序网络；
- `sort` 在原实现中的作用只是满足 `set_difference` 的有序输入前提；
- bitmap 构造阶段仍是标量，承担无序 / 重复 selected indices 的 membership 合并语义；
- 该模式只适用于“全集是 `[0,n)` 且目标是 membership complement”的场景，不能推广为通用排序或任意集合差；
- 若后续接入生产，必须把 bitmap 初始化 / 标记成本、内存流量、越界检查、重复 indices、后续 `copyPointCloud` 或字段写回全部纳入 full diagnostic / production bench，而不能只引用 bitmap scan 局部收益。

因此，本文档后续把 `complementBitmapScanRVV` 称为“bitmap scan + compress”，而不是“sort 的 RVV 实现”。

### 4.1 特殊实体说明

| 实体                            | 类型                   | 输入 / 输出                               | 作用与边界                                                                                     |
| ------------------------------- | ---------------------- | ----------------------------------------- | ---------------------------------------------------------------------------------------------- |
| `complementSetDifferenceStd`  | 标量 helper            | `input_size + selected -> complement`   | 对应上游 `full_indices + sort + set_difference`，作为对拍基准                                |
| `buildMembershipBitmap`       | 标量 helper            | `input_size + selected -> uint8 bitmap` | 把 selected 标为 1；无序 / 重复 indices 在这里收敛，避免 RVV 冲突写语义                        |
| `complementBitmapStd`         | 标量 helper            | bitmap scan -> complement                 | 诊断 bitmap 算法的标量版本                                                                     |
| `complementBitmapScanRVV`     | RVV 诊断 helper        | bitmap -> complement                      | `vle8` 读取 bitmap，`bitmap==0` 生成 mask，`vid+base` 生成 index，`vcompress` 保序输出 |
| `complementBitmapRVV`         | RVV 诊断 wrapper       | `input_size + selected -> complement`   | 标量 bitmap 构造 + RVV scan；小规模或超 32 位范围 fallback                                     |
| `setPointXYZFieldsStd`        | 标量 helper            | cloud + indices + value -> cloud          | 写 `PointXYZ::x/y/z`，模拟 keep_organized / filterDirectly 的字段写坏点                      |
| `scatterSetPointXYZFieldsRVV` | RVV 诊断 helper        | cloud + indices + value -> cloud          | 用公共 `rvv_point_store` scatter wrapper 写 `x/y/z`                                        |
| `keepOrganizedPointXYZRVV`    | full diagnostic helper | input + selected + mode -> output         | positive 模式先求 complement，negative 模式直接写 selected；只覆盖 `PointXYZ` 诊断           |

这些实体不改变 `ExtractIndices` 的对象状态，不暴露为公开 API。`buildMembershipBitmap` 是有意保留的标量阶段：它承担重复 / 无序 selected indices 的冲突合并语义，RVV scan 只负责线性生成补集。

### 4.2 complement bitmap scan

每个 VL chunk 中，bitmap 的 0 表示该 source index 不在 selected 集合中，应进入补集。`vid.v` 生成 chunk 内 lane id，加上 chunk 起点得到全局 index；`vcompress` 按 mask 把 absent lane 保序压缩，最后一次 `vse32` 写入补集 staging。

```cpp
const std::size_t vl = __riscv_vsetvl_e8m1(n - i);
const vuint8m1_t flags = __riscv_vle8_v_u8m1(bitmap.data() + i, vl);
const vbool8_t is_absent = __riscv_vmseq_vx_u8m1_b8(flags, 0, vl);
const vuint32m4_t local = __riscv_vid_v_u32m4(vl);
const vuint32m4_t global = __riscv_vadd_vx_u32m4(local, static_cast<std::uint32_t>(i), vl);
const vuint32m4_t compact = __riscv_vcompress_vm_u32m4(global, is_absent, vl);
const std::size_t keep_count = __riscv_vcpop_m_b8(is_absent, vl);

// Absent bits become monotonically increasing indices.  vcompress preserves
// that order, matching set_difference over the sorted full index range.
__riscv_vse32_v_u32m4(out + kept, compact, keep_count);
```

等价条件：

- bitmap 的 bit `i` 已准确表示 `i` 是否在 selected 集合中；
- scan 从 `0` 到 `n-1` 单调前进；
- `vcompress` 保留 absent lane 的相对顺序；
- 输出类型仍是 `pcl::Indices` 的 32 位 index。

### 4.3 keep_organized sparse 写坏点

`keep_organized` 标量路径会复制整云，再把 removed 位置每个字段写为 `user_filter_value_`。诊断 helper 固定 `PointXYZ`，只写 `x/y/z` 三个 float 字段。indices 是 sparse point id，先转成 `PointXYZ` AoS byte offset，再通过公共 `rvv_point_store` scatter wrapper 写字段。

```cpp
const vint32m2_t v_indices = __riscv_vle32_v_i32m2(indices.data() + i, vl);
const vuint32m2_t v_indices_u = __riscv_vreinterpret_v_i32m2_u32m2(v_indices);
const vuint32m2_t offsets = pcl::rvv_load::byte_offsets_u32m2<pcl::PointXYZ>(v_indices_u, vl);
const vfloat32m2_t values = __riscv_vfmv_v_f_f32m2(user_value, vl);

// Sparse bad-value writes target existing point slots.  The common scatter
// store wrapper keeps the PointXYZ field layout explicit and avoids inventing
// a production-only store path for this diagnosis.
pcl::rvv_store::scatter_store3_f32m2<offsetof(pcl::PointXYZ, x),
                                     offsetof(pcl::PointXYZ, y),
                                     offsetof(pcl::PointXYZ, z)>(
    base, offsets, vl, values, values, values);
```

该诊断不能代表泛型生产路径的全部成本，因为生产 `PointT` 可能有更多 float 字段或非 float 字段，`PCLPointCloud2` 还需要按 `fields[j].offset` 和 `point_step` 写字节云。

## 5. 数值算例与 VL chunk 图示

设输入点云有 8 个点，`selected = [1, 4, 6]`。标量补集是：

```text
full_indices    = [0,1,2,3,4,5,6,7]
selected sorted = [1,4,6]
complement      = [0,2,3,5,7]
```

bitmap 形式：

|      source index | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 |
| ----------------: | -: | -: | -: | -: | -: | -: | -: | -: |
|      selected bit | 0 | 1 | 0 | 0 | 1 | 0 | 1 | 0 |
|       absent mask | T | F | T | T | F | T | F | T |
| compressed output | 0 | 2 | 3 | 5 | 7 |   |   |   |

图示：

```text
bitmap lanes:   [0,1,0,0,1,0,1,0]
vid + base:     [0,1,2,3,4,5,6,7]
mask == 0:      [T,F,T,T,F,T,F,T]
vcompress:      [0,2,3,5,7]
```

对 `keep_organized=false` 的 `negative=true`，这些 complement indices 之后仍要进入 `copyPointCloud`。对 `keep_organized=true` 的 `negative=false`，这些 complement indices 是被写坏点的位置；对 `negative=true`，写坏点位置则是原 selected indices。

## 6. 测试、QEMU、反汇编和板卡证据

专项测试：

- `make -C test-rvv/filters/extract_indices run_test_compare` 通过，std/RVV 两套构建均通过 6 个测试；
- 测试覆盖 bitmap 补集、RVV 补集对拍、positive / negative sparse 写坏点、先 RVV 后小规模 fallback、未修改生产入口可运行。

QEMU bench：

- `make -C test-rvv/filters/extract_indices run_bench_compare` 通过；
- `output/qemu/analyze_bench_compare.log` 无 `未解析`、`n/a`、`Total Time 不计算`；
- QEMU 只作为构建、checksum、格式和指令路径证据，不作为性能结论。

QEMU 诊断结果摘要：

| case                               | QEMU 现象                      | 解释                                             |
| ---------------------------------- | ------------------------------ | ------------------------------------------------ |
| complement sparse 64K / 1M         | checksum 对齐，RVV 日志较短    | 只说明局部 bitmap scan + compress 正确且路径命中 |
| complement half 1M                 | checksum 对齐，RVV 日志较短    | 半数输出时压缩仍保持顺序                         |
| keep organized positive / negative | checksum 对齐，QEMU 未显示收益 | sparse scatter 写坏点不是明确强模式              |
| production unchanged               | std/RVV 约 `1.00x`           | 生产入口未被本主题改变                           |

反汇编：

- `make -C test-rvv/filters/extract_indices dump_bench_rvv` 生成 `build/asm/riscv/bench_extract_indices_rvv.full.asm`；
- `output/qemu/rvv_asm_check.log` 确认 `vle8.v`、`vmseq.vi`、`vid.v`、`vcompress.vm`、`vcpop.m`、`vse32.v`、`vsuxseg3ei32.v`、`vsetvli`。

上游测试：

- `make -C test-rvv/filters/extract_indices run_upstream_test_compare` 通过；
- 上游 `test/filters/test_filters.cpp` 使用仓库已有 `test/bun0.pcd` 与 `test/milk_cartoon_all_small_clorox.pcd` 作为参数，std/RVV 均通过 21 项。

板卡验证：

- `make -C test-rvv/filters/extract_indices run_board_test run_board_bench_compare fetch_board_logs` 通过；
- 板卡专项测试 6 项通过；
- 日志位于 `test-rvv/filters/extract_indices/output/board/`。

Milkv-Jupiter 结果：

| case                                                        |         Std |         RVV |   speedup | 说明                                              |
| ----------------------------------------------------------- | ----------: | ----------: | --------: | ------------------------------------------------- |
| `extract indices complement diag sparse 64K`              |  `1.7946` |  `0.5110` | `3.51x` | 局部 bitmap scan + compress，证明补集尾段可批量化 |
| `extract indices complement diag sparse 1M`               | `23.3869` | `10.5958` | `2.21x` | 1M 稀疏 selected 的局部补集生成                   |
| `extract indices complement diag half 1M`                 | `40.3241` |  `8.3397` | `4.84x` | 半数 selected 下输出比例变化，仍保持 checksum     |
| `extract indices keep organized positive diag 64K`        |  `4.0083` |  `3.2866` | `1.22x` | 补集 +`PointXYZ` 坏点写 full diagnostic，弱收益 |
| `extract indices keep organized positive diag 1M`         | `55.4858` | `44.6745` | `1.24x` | 1M full diagnostic，弱收益                        |
| `extract indices keep organized negative diag 1M`         | `33.4239` | `33.8247` | `0.99x` | selected sparse scatter 写坏点不成立              |
| `extract indices production unchanged negative 64K`       |  `3.8180` |  `3.8536` | `0.99x` | 未修改生产入口，不作为新增 RVV 结论               |
| `extract indices production unchanged keep organized 64K` |  `4.8987` |  `4.9617` | `0.99x` | 未修改生产入口，不作为新增 RVV 结论               |

## 7. 当前结论

当前主题保持 bench 诊断，不接入生产路径。bitmap scan + `vcompress` 是正确且在板卡上明显加速的局部替代片段，但生产 `ExtractIndices` 的主成本还包括 bitmap 构造、sort / set_difference 语义边界、`copyPointCloud`、全云复制、所有字段坏点写回和 `PCLPointCloud2` 字节布局。板卡 full diagnostic 只有弱收益或退化，未修改生产入口为 `0.99x`，因此当前不值得引入生产分流和维护边界。
