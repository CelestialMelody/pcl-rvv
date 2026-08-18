# filters/extract_indices 函数级 RVV 诊断评估

## 1. 主题状态

| 项目 | 状态 |
| --- | --- |
| 模块执行来源 | `doc-rvv/library-screening/filters/filters-retained-candidate-rescreen.zh.md` 的 `6.2 暂缓 / 不单独实施（诊断路径记录）` |
| 当前定位 | bench 诊断，不修改生产源码 |
| 生产源码 | `filters/include/pcl/filters/impl/extract_indices.hpp`、`filters/src/extract_indices.cpp` 保持不变 |
| 专项路径 | `test-rvv/filters/extract_indices/` |
| 主题文档 | `doc-rvv/filters/extract_indices-RVV.zh.md` |

`extract_indices` 来自保留候选复筛的 diagnostic / bench-only 路径记录。当前工作不接入 `pcl::ExtractIndices` 生产分流，而是隔离两个局部问题：`full_indices + sort + set_difference` 是否可由 bitmap scan + RVV compress 替代，以及 `keep_organized` / `filterDirectly` 的 sparse bad-value 写回是否值得 RVV scatter。

## 2. 函数入口与标量语义

| 入口 | 标量职责 | 诊断结论 |
| --- | --- | --- |
| `ExtractIndices<PointT>::applyFilterIndices(Indices&)` | `negative=false` 时直接返回 `indices_`；需要 removed 或 `negative=true` 时构造全量 `[0,n)`、排序输入 indices、用 `std::set_difference` 求补集 | 可诊断补集生成局部片段；生产语义涉及无序 / 重复 indices，暂不接入 |
| `ExtractIndices<PointT>::applyFilter(PointCloud&)` | 非 organized 时调用 `applyFilterIndices` 后 `copyPointCloud`；organized 时复制整云并把 removed 位置所有字段写为 `user_filter_value_` | 整点复制和所有字段写回主导，当前诊断范围为 `PointXYZ x/y/z` sparse 写坏点 |
| `ExtractIndices<PointT>::filterDirectly(PointCloudPtr&)` | 原地把 removed 点的所有字段写为 `user_filter_value_` | 与 organized sparse 写回同类；生产泛型字段列表暂不 RVV 化 |
| `ExtractIndices<PCLPointCloud2>::applyFilter` | 字节云按 `point_step` 拷贝或按字段 offset 写坏点 | 字段数量和字节布局更泛化，本轮不做 RVV |

标量补集路径的关键步骤是：

```text
full_indices = [0, 1, ..., n-1]
sorted_input_indices = sort(indices_)
complement = set_difference(full_indices, sorted_input_indices)
```

诊断 RVV 使用相同输入 `selected` 先构造 scalar membership bitmap，再用 RVV 扫描 bitmap 中为 0 的位置，输出递增补集 indices。bitmap 标记阶段仍是标量，原因是输入 indices 可能无序且有重复，直接在 RVV 中解决冲突写会引入额外语义和成本；本诊断只评估 `set_difference` 尾段能否被 bitmap scan + compress 替代。

这不是 RVV 化 `sort`。原标量 `sort` 的作用是让 `set_difference` 可以在两个有序范围上工作；对于 `ExtractIndices` 的补集问题，真正需要的是判断每个 source index 是否属于 selected 集合。诊断路径把“排序后求差”改写为“membership bitmap + 按 `[0,n)` 递增扫描”，因此 RVV 只负责线性扫描 bitmap、生成递增 source index 并用 `vcompress` 保序输出。该算法等价改写只适用于全集为连续整数 `[0,n)` 的补集问题，不能推广为通用 RVV sort。

## 3. RVV 覆盖与 fallback

| 项目 | 结论 |
| --- | --- |
| 覆盖点类型 | 诊断固定 `pcl::PointXYZ` |
| 覆盖数据形态 | `input_size < 2^32`，selected indices 已由 scalar bitmap 处理 |
| RVV 片段 1 | `bitmap[i] == 0` mask、`vid + base`、`vcompress`、contiguous `vse32` 写补集 indices |
| RVV 片段 2 | 对 selected 或补集位置 scatter 写 `PointXYZ::x/y/z = user_filter_value` |
| 保持标量 | bitmap 构造、生产 `ExtractIndices` 入口、`copyPointCloud`、所有泛型字段写回、PCLPointCloud2 |
| fallback | 小规模、非 RVV 编译、超出 32 位 lane 可表达范围、生产入口均走标量 |
| FRM/FCSR | 不涉及 float-to-int 或显式舍入模式，不修改 FRM/FCSR |

## 4. 中优先级尝试与暂缓原因

本主题属于保留候选复筛中的 diagnostic / bench-only 路径记录，默认不接入生产。已经尝试的 RVV 点如下：

| 尝试项 | 实现状态 | 风险 / 处理 |
| --- | --- | --- |
| bitmap 补集 scan | 已实现诊断 RVV | 可证明局部 checksum 一致，但不包含 bitmap 构造、后续 copyPointCloud 或 keep_organized 全字段写回 |
| keep_organized `PointXYZ` sparse 写坏点 | 已实现诊断 RVV | 只覆盖 `x/y/z` 三个 float 字段；生产泛型需要 `FieldList` 所有字段，PCLPointCloud2 还涉及字段 offset 和 point_step |
| 生产 `applyFilterIndices` 分流 | 暂缓 | 输入 indices 无序 / 重复时需要保持 `set_difference` 语义；bitmap 构造成本和内存流量可能抵消局部收益 |
| 生产 `applyFilter(PointCloud&)` / `filterDirectly` 分流 | 暂缓 | 整点复制、字段列表循环和 keep_organized 全云拷贝可能是主成本，局部 scatter 不足以证明生产收益 |

## 5. 专项测试与 bench 计划

专项测试：

- bitmap 补集结果与 `std::set_difference` 对齐；
- RVV bitmap scan 与标量补集对齐；
- `keep_organized` positive / negative 写坏点与标量 `PointXYZ` 诊断对齐；
- 同一进程先运行 RVV 主片段再运行小规模 fallback，确认无后续标量污染；
- 未修改的生产 `ExtractIndices<PointXYZ>` 仍可运行。

bench case：

| case | 含义 | 是否生产结论 |
| --- | --- | --- |
| `extract indices complement diag sparse 64K/1M` | 稀疏 selected 下的补集尾段诊断 | 否，仅局部片段 |
| `extract indices complement diag half 1M` | 半数 selected 下的补集尾段诊断 | 否，仅局部片段 |
| `extract indices keep organized positive diag 64K/1M` | positive 模式下补集 + sparse 写坏点诊断 | 否，缺少泛型字段和生产全路径 |
| `extract indices keep organized negative diag 1M` | negative 模式下 selected sparse 写坏点诊断 | 否，缺少泛型字段和生产全路径 |
| `extract indices production unchanged ...` | 未修改生产入口观察 | 否，用于确认无生产分流变化 |

## 6. 当前验证状态

| 项目 | 状态 |
| --- | --- |
| QEMU 专项测试 | `make -C test-rvv/filters/extract_indices run_test_compare` 通过，std/RVV 均 6 项通过 |
| QEMU bench | `make -C test-rvv/filters/extract_indices run_bench_compare` 通过 |
| bench 解析 | `output/qemu/analyze_bench_compare.log` 无 `未解析`、`n/a`、`Total Time 不计算` |
| 反汇编 | `output/qemu/rvv_asm_check.log` 确认 `vle8`、`vmseq`、`vid.v`、`vcompress.vm`、`vcpop.m`、`vse32.v`、`vsuxseg3ei32.v` |
| 上游测试 | `make -C test-rvv/filters/extract_indices run_upstream_test_compare` 通过，std/RVV 均通过 `test_filters.cpp` 21 项 |
| 板卡验证 | `make -C test-rvv/filters/extract_indices run_board_test run_board_bench_compare fetch_board_logs` 通过，日志位于 `output/board/` |

QEMU 输出只作为构建、checksum、日志格式和指令路径证据，不作为真实性能结论。

板卡结果：

| case | Milkv-Jupiter 结果 | 结论 |
| --- | --- | --- |
| `extract indices complement diag sparse 64K` | Std `1.7946` ms/iter，RVV `0.5110` ms/iter，`3.51x` | 局部 bitmap scan + compress 成立 |
| `extract indices complement diag sparse 1M` | Std `23.3869` ms/iter，RVV `10.5958` ms/iter，`2.21x` | 局部片段成立，但不含后续 copy |
| `extract indices complement diag half 1M` | Std `40.3241` ms/iter，RVV `8.3397` ms/iter，`4.84x` | 输出比例变化下 `vcompress` 路径成立 |
| `extract indices keep organized positive diag 64K` | Std `4.0083` ms/iter，RVV `3.2866` ms/iter，`1.22x` | full diagnostic 弱收益 |
| `extract indices keep organized positive diag 1M` | Std `55.4858` ms/iter，RVV `44.6745` ms/iter，`1.24x` | full diagnostic 弱收益 |
| `extract indices keep organized negative diag 1M` | Std `33.4239` ms/iter，RVV `33.8247` ms/iter，`0.99x` | sparse scatter 写坏点不成立 |
| `extract indices production unchanged negative 64K` | Std `3.8180` ms/iter，RVV `3.8536` ms/iter，`0.99x` | 未修改生产入口，无新增生产收益 |
| `extract indices production unchanged keep organized 64K` | Std `4.8987` ms/iter，RVV `4.9617` ms/iter，`0.99x` | 未修改生产入口，无新增生产收益 |

## 7. 当前接入判断

当前保持 bench 诊断，不接入生产路径。板卡显示 bitmap scan 局部片段有 `2.21x` 到 `4.84x`，但接近 keep_organized 的 full diagnostic 只有 `1.22x` 到 `1.24x` 或 `0.99x`，未修改生产入口为 `0.99x`。生产接入还需要处理 bitmap 构造、整点复制、所有字段写回、无序 / 重复 indices 语义和 `PCLPointCloud2`，当前收益不足以承担这些维护成本。
