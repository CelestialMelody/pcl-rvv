# filters/radius_outlier_removal RVV 诊断说明

## 1. 函数入口作用

`pcl::RadiusOutlierRemoval<PointT>` 根据指定半径内的邻居数量筛除离群点。公开入口 `filter(indices)` 最终进入 `RadiusOutlierRemoval<PointT>::applyFilterIndices(Indices&)`，输出通过半径邻居条件的原始点索引，并可在 `extract_removed_indices_` 打开时记录被移除的索引。

该入口的主成本不是线性 predicate，而是对每个点执行空间搜索：

```text
dense input:
  nearestKSearch(index, min_pts_radius + 1)
  compare last neighbor distance with search_radius^2
  mark to_keep[i]

non-dense input:
  isXYZFinite(input[index])
  radiusSearch(index, search_radius, max_nn=min_pts_radius + 1)
  mark to_keep[i]

tail:
  compress to_keep into output indices and optional removed_indices
```

本主题来自保留候选复筛的 diagnostic / bench-only 路径记录。当前不修改 `filters/include/pcl/filters/impl/radius_outlier_removal.hpp` 或 `filters/src/radius_outlier_removal.cpp`，只在 `test-rvv/filters/radius_outlier_removal/` 中保留诊断 helper、测试和 bench。

## 2. 标量路径与诊断边界

上游标量路径先生成 `to_keep`，再执行尾段压缩：

```text
to_keep[i] == 1 -> output indices.push_back(indices_[i])
to_keep[i] == 0 -> optional removed_indices.push_back(indices_[i])
```

保留候选复筛将本主题列为 `保留 / 待诊断`，诊断点是 `to_keep` 到 `indices` / `removed_indices_` 的尾段压缩。保留原因是 `nearestKSearch` / `radiusSearch` 主导生产入口，尾段压缩只能证明后处理成本上界。

本轮诊断拆成两个层次：

| 层次 | 对应 bench case | 说明 |
| --- | --- | --- |
| local correctness / microbench | `radius_outlier_removal tail compress ...` | 只测 `to_keep` 到 kept/removed indices 的线性压缩 |
| entrance/full diagnostic | `radius_outlier_removal full diag ...` | 在 synthetic cloud 上模拟 search-dominated 前段，再接相同 tail compress |
| entry smoke | gtest `EntryDiagnosticMatchesUpstream...` | test-only 子类走真实上游公开设置和 `filter(indices)`，只验证诊断入口形状与上游一致，不替换 RVV |

full diagnostic 不是 PCL searcher 的替代实现，只用于回答“如果 search 仍然占主要成本，尾段 RVV 是否还能穿透完整入口”。

按当前 agent 规则，本主题的 `diagnostic_entry_strategy` 是：

| 策略 | 证据质量 | 说明 |
| --- | --- | --- |
| `replay-context` | `local-fragment` / `bench-only` | 当前 RVV 候选只覆盖尾段压缩，显式映射 `indices_`、`removed_indices_`、`extract_removed_indices_` 和局部 `to_keep` |
| `subclass-original` | entry smoke | `RadiusOutlierRemovalEntryDiagnostic<PointT>` 继承上游目标类，公开同名同参数 `applyFilterIndices(Indices&)` 诊断入口，并通过公开 setter 验证 fake indices / explicit indices 调用形状 |

`subclass-original` smoke 没有替换 search 或 tail，因此不能作为 RVV production-shaped 收益证据；它只记录如果后续要做 production-shaped 诊断，应从真实入口生命周期开始，而不是只继续扩大 microbench。

## 3. 覆盖范围与 fallback

| 项目 | 结论 |
| --- | --- |
| 覆盖输入 | `std::vector<uint8_t> to_keep` 和 `pcl::Indices source_indices` |
| RVV 内容 | `vle8` 读取 keep mask，`vle32` 读取 source index，`vcompress` 分别输出 kept / removed |
| 保持标量 | 空间搜索、finite 检查、dense / non-dense predicate、`negative_` 语义、生产入口 |
| fallback | 小规模、非 RVV 编译、输入尺寸不匹配 |
| 生产入口 | 不修改公开 API，不接入生产分流 |
| FRM/FCSR | 不涉及显式舍入模式 |

## 4. 详细设计

专项实现位于：

- `test-rvv/filters/radius_outlier_removal/radius_outlier_removal_replay.hpp`
- `test-rvv/filters/radius_outlier_removal/radius_outlier_removal_diag.hpp`

主要实体：

| 实体 | 类型 | 作用 |
| --- | --- | --- |
| `ApplyFilterIndicesTailReplayContext` | replay context | 把上游 `indices_`、`extract_removed_indices_` 和局部 `to_keep` 映射为诊断字段 |
| `ApplyFilterIndicesTailResult` | 诊断结果结构 | 保存 kept 和 removed indices |
| `applyFilterIndicesTailStdReplay` | 标量 replay | 对应上游 `applyFilterIndices` 尾段压缩语义 |
| `applyFilterIndicesTailRVVReplay` | RVV replay | 扫描 `to_keep`，用 `vcompress` 生成 kept/removed |
| `applyFilterIndicesTailAutoReplay` | wrapper | RVV 成功则使用 RVV，否则 fallback 到 Std |
| `RadiusOutlierRemovalEntryDiagnostic` | test-only 子类 | 继承上游目标类，提供 `applyFilterIndices(Indices&)` 同形诊断入口，用于 entry smoke |
| `simulateApplyFilterIndicesSearchPhase` | 标量模拟前段 | 用 synthetic cloud 模拟 search 主导成本，不代表 PCL searcher |
| `runApplyFilterIndicesDiagnostic` | full diagnostic helper | 前段模拟 + tail compress，用于收益穿透判断 |

RVV 实际覆盖的原标量范围是 `filters/include/pcl/filters/impl/radius_outlier_removal.hpp` 中 `applyFilterIndices(Indices&)` 的尾段压缩循环，位于 search / predicate 填好 `to_keep` 之后、`indices.resize(oii)` 与 `removed_indices_->resize(rii)` 之前。

RVV 实际接管：

- 读取 `to_keep[i]`；
- 读取 `(*indices_)[i]` 对应的 source index；
- 对 `to_keep != 0` 生成 kept mask；
- 用 `vcompress` 保序写出 kept indices；
- 在 `extract_removed_indices_ == true` 时用反 mask 保序写出 removed indices。

仍保持标量或不在本诊断中覆盖：

- `searcher_` 初始化和 `setInputCloud`；
- dense 路径 `nearestKSearch`；
- non-dense 路径 `isXYZFinite` 和 `radiusSearch`；
- `negative_` 对 `to_keep` 的影响；
- `PCLPointCloud2` 字节云路径；
- 生产 `filter(indices)` / `applyFilterIndices` 分流。

RVV helper 每个 VL chunk 执行：

```text
v_keep = vle8(to_keep + i)
m_keep = v_keep != 0
v_src  = vle32(source_indices + i)
kept   = vcompress(v_src, m_keep)
removed = vcompress(v_src, !m_keep)  # extract_removed_indices=true 时
```

`vcompress` 保留 lane 相对顺序，因此 kept / removed 输出顺序与标量尾段一致。诊断 helper 不处理 searcher，也不把 `negative_` predicate 放进 RVV；`negative_` 在上游语义中已经体现在 `to_keep` 的生成结果里。

## 5. 数值算例与 VL chunk 图示

设一个 chunk 中：

```text
source_indices: [10, 11, 12, 13, 14, 15, 16, 17]
to_keep:        [ 1,  0,  1,  1,  0,  0,  1,  1]
keep mask:      [ T,  F,  T,  T,  F,  F,  T,  T]
removed mask:   [ F,  T,  F,  F,  T,  T,  F,  F]
kept output:    [10, 12, 13, 16, 17]
removed output: [11, 14, 15]
```

这个图示只描述 search 后处理。真实入口前半段仍需要为每个 index 访问 searcher 并决定 `to_keep[i]`。

## 6. 测试、QEMU、反汇编和板卡证据

专项测试：

- `make -C test-rvv/filters/radius_outlier_removal run_test_compare` 通过；
- std/RVV 两套构建均通过 8 个 gtest；
- 覆盖 mostly-keep、half-keep shuffled、no-removed、小规模 fallback、full diagnostic checksum、未修改生产入口、test-only 子类 fake indices smoke、explicit indices + negative smoke。

QEMU bench：

- `make -C test-rvv/filters/radius_outlier_removal run_bench_compare` 通过；
- `output/qemu/analyze_bench_compare.log` 可解析；
- QEMU 只作为构建、checksum、格式和指令路径证据，不作为真实性能结论。

反汇编：

- `make -C test-rvv/filters/radius_outlier_removal dump_bench_rvv` 生成 `build/asm/riscv/bench_radius_outlier_removal_rvv.full.asm`；
- 摘录中可见 `vle8.v`、`vmsne.vi`、`vcompress.vm`、`vcpop.m`、`vse32.v`、`vsetvli`；
- 二进制还包含其它自动向量化或库路径指令，不能把全部 RVV 指令归因到本主题 helper。

板卡验证：

- `make -C test-rvv/filters/radius_outlier_removal run_board_test fetch_board_logs` 在补充 entry smoke 后通过，`output/board/run_test.log` 显示 8 个 gtest 通过；
- 性能表继续使用此前 `run_board_bench_compare` 收集的 bench 日志；本轮新增 entry smoke 不改变 bench case；
- 日志位于 `test-rvv/filters/radius_outlier_removal/output/board/`；
- 设备：Milkv-Jupiter；iterations：5。

## 7. 板卡结果与 case 解释

speedup 计算方式为 `Std avg ms/iter / RVV avg ms/iter`。

| case | 路径含义 | speedup | 证明点 |
| --- | --- | ---: | --- |
| `tail compress mostly-keep 64K` | kept 和 removed 都输出，多数保留 | 0.96x | tail-only 不成立 |
| `tail compress half-keep shuffled 64K` | shuffled source indices，约半数保留 | 0.99x | 基本持平 |
| `tail compress mostly-remove 64K` | 多数移除 | 1.02x | 弱收益 |
| `tail compress mostly-keep 1M` | 1M，多数保留 | 0.91x | tail-only 退化 |
| `tail compress half-keep shuffled 1M` | 1M shuffled，约半数保留 | 1.03x | 弱收益 |
| `tail compress no-removed 1M` | 只输出 kept，不写 removed | 1.32x | 局部单变体有收益 |
| `full diag light-search 64K` | 轻量前段 + tail | 1.07x | 弱收益 |
| `full diag search-dominated 64K` | search-dominated 前段 + tail | 1.02x | 收益被前段稀释 |
| `full diag search-dominated shuffled 64K` | shuffled 前段 + tail | 1.00x | 持平 |
| `full diag search-dominated 1M` | 1M search-dominated 前段 + tail | 0.99x | full diagnostic 不成立 |
| `full diag search-dominated mostly-remove 1M` | 1M 多数移除 | 0.99x | full diagnostic 不成立 |

## 8. 结论

`radius_outlier_removal` 当前完成 bench-only 诊断，生产不接入。RVV tail-compress helper 的语义对拍成立，指令路径命中，但板卡 full diagnostic 基本持平或略慢，不能证明生产入口收益。

生产接入不成立的直接原因是：真实入口由 `nearestKSearch` / `radiusSearch` 主导，尾段压缩占比太小；tail-only case 也不稳定，只有 no-removed 1M 单一变体达到 `1.32x`。当前证据不足以承担生产分流、fallback 和维护成本。

后续若重新评估，应先取得真实 profile，证明 search 后处理已经成为瓶颈，或先在 search 阶段形成可观收益后再复核尾段压缩。若要把本主题升级为 production-shaped 诊断，应以 `RadiusOutlierRemovalEntryDiagnostic` 这类 `subclass-original` 入口为起点，覆盖 fake indices、显式 subset、`negative_`、removed indices 和真实 searcher 生命周期；只扩大 tail microbench 不足以改变生产接入结论。
