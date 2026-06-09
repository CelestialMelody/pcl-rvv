# filters/conditional_removal RVV 优化说明

## 1. 函数入口在 PCL 中的作用

`pcl::ConditionalRemoval<PointT>` 是按用户定义条件筛选点云的 filters 入口。调用者先构造 `ConditionAnd` / `ConditionOr` 条件树，把 `FieldComparison`、RGB/HSI 比较或二次曲面比较加入条件，再调用 `filter(output)`。`Filter<PointT>::filter` 完成输入检查后调用 `ConditionalRemoval<PointT>::applyFilter(PointCloud&)`。

该入口输入为 `input_` 点云和可选 `indices_`，输出为满足条件的 `PointCloud<PointT>`。当 `extract_removed_indices_` 打开时，未通过条件或非 finite xyz 的点 index 写入 `removed_indices_`。当 `keep_organized_` 打开时，输出保持原 organized 形状，未保留点写为 `user_filter_value_`。

本主题优化的是直接筛选主路径：全云、dense、`!keep_organized_`、单个 `FieldComparison<float>`。这类形态常见于“按 z / intensity / 某个 float 字段阈值过滤点云”，原标量每点经 `condition_->evaluate(point)`、`PointDataAtOffset::compare` 和 `copyPoint` 处理。

## 2. 标量路径与 RVV 覆盖范围

标量路径核心语义：

```cpp
for (std::size_t index: (*Filter<PointT>::indices_))
{
  const PointT& point = (*input_)[index];
  if (!std::isfinite (point.x) || !std::isfinite (point.y) || !std::isfinite (point.z))
  {
    if (extract_removed_indices_) (*removed_indices_)[nr_removed_p++] = index;
    continue;
  }
  if (condition_->evaluate (point))
    copyPoint (point, output[nr_p++]);
  else if (extract_removed_indices_)
    (*removed_indices_)[nr_removed_p++] = index;
}
```

RVV 覆盖条件：

- `PointT` 为 standard-layout，且有 `float x/y/z`；
- 输入 dense，未显式 `setIndices()`，`keep_organized_ == false`；
- 点数不少于 `64`；
- `condition_` 是 `ConditionAnd<PointT>`，没有嵌套 condition，且只有一个 `FieldComparison<PointT>`；
- comparison 字段 datatype 为 `PCLPointField::FLOAT32`；
- op 为 `GT/GE/LT/LE`。

fallback 条件：

- 小规模、non-dense、subset indices、`keep_organized_`；
- `ConditionOr`、复合条件、嵌套 condition；
- packed RGB/HSI、TfQuadraticXYZ、非 float 字段；
- float `EQ`。原因是 `PointDataAtOffset::compare` 对 NaN 字段的 `EQ` 会因 `>` 和 `<` 均为 false 而等价为 compare_result `0`，直接用 `vmfeq` 不等价。

## 3. 详细设计

新增实体：

- `applyFilterStd`：常驻标量 helper，承载原 `applyFilter` 主体；
- `applyFilterRVV`：`__RVV10__` 下的 RVV helper，执行 VL chunk 字段比较和 kept/removed index 压缩；
- `ConditionalRemovalXYZCompatible`：语义 traits，只判断 `PointT` 是否具备可安全 stride-load 的 `float x/y/z`；
- `compareFloatFieldMask`：把 `GT/GE/LT/LE` 映射成 RVV float mask；
- `friend class ConditionalRemoval`：让分流逻辑读取 `ConditionBase` / `FieldComparison` 已有 metadata，避免新增公开 getter。

分流前条件识别：

`applyFilter` 在调用 `applyFilterRVV` 前会先做一组看起来比普通短路分发更多的检查：确认 `PointT` 的 xyz layout、`capable_` / `input_` / `condition_` 有效，识别 `condition_` 是否为 `ConditionAnd<PointT>`，再读取其受保护的 `conditions_` / `comparisons_`，确认没有嵌套条件且只有一个 `FieldComparison<PointT>`。随后分流逻辑检查该 comparison 的 `point_data_` 是否存在、字段 datatype 是否为 `FLOAT32`、比较操作是否为 `GT/GE/LT/LE`、比较值是否可用 `float` 表达，并把原 comparison 中的 `field_offset`、`op`、`compare_val` 展开为传给 RVV helper 的标量参数。

这部分工作是每次 `filter()` 调用一次的语义识别和 metadata 展开，不在逐点 / 逐 VL chunk 循环中执行。它的作用是证明当前条件树等价于标量公式 `keep = finite(x,y,z) && field op compare_val`，并把原对象状态预取成 RVV mask 计算所需参数；`field_offset`、`op`、`compare_val` 都来自用户原本构造的 `FieldComparison`，不是新增过滤语义。任何一步识别失败都会回退 `applyFilterStd`，继续走原来的 `condition_->evaluate(point)` 和 `PointDataAtOffset::compare` 路径。

访存与 mask 组织：

- `x/y/z` 使用 `pcl::rvv_load::strided_load3_f32m2`；
- 目标字段使用 `pcl::rvv_load::strided_load_f32m2`；
- finite mask 复刻标量 `std::isfinite(x/y/z)` 检查：`v == v` 排除 NaN，`abs(v) < inf` 排除 +/-Inf；
- 字段比较 mask 与 finite mask 合并得到 `keep`；
- `vcompress` 把 kept index 压缩到临时 buffer，然后逐 index `copyPoint`，保持完整点字段复制和输出顺序；
- `extract_removed_indices_` 打开时，`drop = !keep` 也用 `vcompress` 写入 removed indices。

核心片段：

```cpp
pcl::rvv_load::strided_load3_f32m2<sizeof (PointT),
                                   offsetof (PointT, x),
                                   offsetof (PointT, y),
                                   offsetof (PointT, z)> (chunk, vl, vx, vy, vz);
const vfloat32m2_t vf =
    pcl::rvv_load::strided_load_f32m2<sizeof (PointT)> (
        reinterpret_cast<const float*> (chunk + field_offset), vl);

vbool16_t finite = __riscv_vmfeq_vv_f32m2_b16 (vx, vx, vl);
finite = __riscv_vmand_mm_b16 (finite, __riscv_vmfeq_vv_f32m2_b16 (vy, vy, vl), vl);
finite = __riscv_vmand_mm_b16 (finite, __riscv_vmfeq_vv_f32m2_b16 (vz, vz, vl), vl);
finite = __riscv_vmand_mm_b16 (finite, abs(vx) < inf, vl);
...
vbool16_t keep = pcl::compareFloatFieldMask (vf, op, compare_val, vl);
keep = __riscv_vmand_mm_b16 (keep, finite, vl);

const vint32m2_t kept_i32 = __riscv_vcompress_vm_i32m2 (source_i32, keep, vl);
__riscv_vse32_v_i32m2 (kept_indices.data (), kept_i32, keep_count);
for (std::size_t k = 0; k < keep_count; ++k)
  copyPoint ((*input_)[kept_indices[k]], output[kept++]);
```

`abs(vx) < inf` 在源码中由 `vfabs` + `vmflt` 表达。这里省略重复代码，只突出有限性、比较和压缩三个语义边界。

## 4. VL chunk 数值算例

设 `PointXYZ` 输入 8 个点，本轮 VL chunk 为 4，条件为 `z > 0.05`：

| lane | input index | x | y | z | finite xyz | z > 0.05 | keep |
| ---: | ---: | ---: | ---: | ---: | --- | --- | --- |
| 0 | 0 | 0.0 | 0.0 | 0.10 | true | true | true |
| 1 | 1 | 1.0 | 0.0 | -0.20 | true | false | false |
| 2 | 2 | NaN | 0.0 | 0.30 | false | true | false |
| 3 | 3 | 2.0 | 0.0 | 0.06 | true | true | true |

VL chunk 图示：

```text
source index : [0, 1, 2, 3]
finite mask  : [1, 1, 0, 1]
cmp mask     : [1, 0, 1, 1]
keep mask    : [1, 0, 0, 1]
vcompress    : [0, 3]
copyPoint    : output += input[0], input[3]
removed      : [1, 2]  // extract_removed_indices_ 打开时
```

标量逐点循环与 RVV chunk 的等价关系是：

```text
keep(i) = isfinite(x_i) && isfinite(y_i) && isfinite(z_i) && compare(field_i, c)
```

其中 `compare` 为 `>`、`>=`、`<`、`<=` 之一。`EQ` 不套用该 RVV 等价式，保持标量。

## 5. 测试、QEMU 与反汇编证据

专项验证：

- `make -C test-rvv/filters/conditional_removal run_test_compare`
- std/RVV 均通过 7 项：主路径、non-finite xyz、`keep_organized` fallback、subset fallback、复合条件 fallback、`EQ` fallback、`PointXYZI intensity`。

上游验证：

- `make -C test-rvv/filters/conditional_removal run_upstream_test_compare`
- 默认参数为 `test/bun0.pcd test/milk_cartoon_all_small_clorox.pcd`。
- std/RVV 两套 `test/filters/test_filters.cpp` 均通过 21 项。

QEMU：

- `make -C test-rvv/filters/conditional_removal run_bench_compare dump_bench_rvv`
- `output/qemu/analyze_bench_compare.log` 可解析，无 `未解析`、`n/a`、`Total Time 不计算`。
- QEMU 只作为构建、正确性、checksum 和指令路径补充，不作为真实性能结论。

反汇编：

- 摘录：`test-rvv/filters/conditional_removal/output/qemu/rvv_asm_check.log`
- 关键指令包含 `vlse32.v`、`vmfeq.vv`、`vmflt.vf`、`vmfgt.vf`、`vmand.mm`、`vcompress.vm`、`vcpop.m`、`vsetvli ... e32,m2`。

## 6. 板卡性能

板卡：Milkv-Jupiter。  
日志：`test-rvv/filters/conditional_removal/output/board/`。  
数据集：synthetic `PointXYZ` / `PointXYZI`，Iterations `30`。  
speedup 计算方式：同一 case 的 `Std Avg(ms) / RVV Avg(ms)`。

| bench case | 函数入口与参数 | 是否命中 RVV | Std ms/iter | RVV ms/iter | speedup | 证明点 |
| --- | --- | --- | ---: | ---: | ---: | --- |
| `conditional_removal single z>0.05 64K` | `ConditionalRemoval<PointXYZ>::filter`，全云，`ConditionAnd` + `FieldComparison("z", GT, 0.05)`，不记录 removed | 是 | 6.9191 | 2.1095 | 3.28x | 小到中等规模主路径收益成立 |
| `conditional_removal single z>0.05 removed 1M` | 同入口，1M 点，记录 `removed_indices_` | 是 | 113.4504 | 39.5442 | 2.87x | kept/drop 双路语义成立，removed 压缩仍有收益 |
| `conditional_removal single y<=0.10 1M` | 全云，`FieldComparison("y", LE, 0.10)` | 是 | 112.4027 | 37.3565 | 3.01x | 不同字段和比较方向仍成立 |
| `conditional_removal pointxyzi intensity>4 1M` | `PointXYZI`，全云，`FieldComparison("intensity", GT, 4)` | 是 | 95.8147 | 20.6196 | 4.65x | 附加 FLOAT32 字段 stride load 成立，整点复制保留字段 |
| `conditional_removal keep-organized fallback 1M` | `keep_organized=true` | 否 | 195.4857 | 204.3193 | 0.96x | 证明未覆盖路径保持语义；不作为 RVV 性能结论 |
| `conditional_removal subset fallback 1M` | 显式 subset indices | 否 | 56.1692 | 55.8958 | 1.00x | 证明 subset 回退路径成本接近；不作为 RVV 性能结论 |

结论：在 Milkv-Jupiter 上，生产 RVV 主路径为 `2.87x` 到 `4.65x`。收益来自把逐点虚接口/metadata compare、有限性检查和保序 index 生成批量化为 VL chunk 上的 stride load、mask 和 `vcompress`；整点复制仍按压缩后的 index 顺序执行，保证输出点云语义不变。
