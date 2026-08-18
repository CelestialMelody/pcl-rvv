# filters/radius_outlier_removal 函数级 RVV 诊断评估

## 1. 主题状态

| 项目 | 状态 |
| --- | --- |
| 模块执行来源 | `doc-rvv/library-screening/filters/filters-second-pass-retained-candidate-rescreen.zh.md` 的 `6.2 暂缓 / 不单独实施（诊断路径记录）` |
| 当前定位 | bench-only 诊断，生产不接入 |
| 生产源码 | `filters/include/pcl/filters/impl/radius_outlier_removal.hpp`、`filters/src/radius_outlier_removal.cpp` 保持不变 |
| 专项路径 | `test-rvv/filters/radius_outlier_removal/` |
| 主题文档 | `doc-rvv/filters/radius_outlier_removal-RVV.zh.md` |

本轮按 `PCL-RVV Single-File Optimization Agent v0` 的 S0-S12 replay 执行，mode 为 `bench-only-diagnostic`。用户授权真实 scaffold、gtest、bench、asm、board 和 closeout；禁止读取 `chats/rvv-workflow-prompt-skill/`，禁止修改 `pcl/agent/`，不进入 registration。

结论：`to_keep` 尾段压缩可以形成 RVV 诊断 helper，checksum 和 gtest 对拍成立，但板卡 full diagnostic 只有 `0.99x` 到 `1.02x`，tail-only case 也不稳定。当前收敛为 bench-only / 生产不接入。

本轮按新版诊断规则补充了 entry strategy 说明和 test-only 子类 smoke：

- 当前 RVV 候选策略：`replay-context`，证据标签为 `local-fragment` / `bench-only`；
- 当前入口 smoke 策略：`subclass-original`，`RadiusOutlierRemovalEntryDiagnostic<PointT>` 继承上游目标类，公开同名同参数 `applyFilterIndices(Indices&)`，并走公开 setter 准备对象状态；
- entry smoke 只证明诊断入口形状可以贴近上游类生命周期，不替换 RVV，也不构成 production-shaped 性能证据。

## 2. 函数入口与标量语义

| 入口 | 标量职责 | 诊断结论 |
| --- | --- | --- |
| `RadiusOutlierRemoval<PointT>::applyFilterIndices(Indices&)` dense 路径 | 对每个输入 index 执行 `nearestKSearch`，根据第 `k-1` 个距离和 `negative_` 标记 `to_keep`，最后把 `to_keep` 压缩到 `indices` / `removed_indices_` | search 主导，尾段压缩只适合做成本上界诊断 |
| `RadiusOutlierRemoval<PointT>::applyFilterIndices(Indices&)` non-dense 路径 | 先检查 finite，再执行 `radiusSearch`，根据邻居数量和 `negative_` 标记 `to_keep`，最后压缩输出 | `radiusSearch` 与 finite/search 结果主导，尾段压缩不能代表生产入口 |
| `RadiusOutlierRemoval<PCLPointCloud2>::applyFilter` | 转为 `PointXYZ` 云后执行 radius search，并复制或组织输出点云 | 涉及字节云和点复制，本轮不诊断 |
| `RadiusOutlierRemoval<PCLPointCloud2>::applyFilter(Indices&)` | PCLPointCloud2 的 indices 输出版本 | 本轮不诊断 |

上游标量尾段是：

```text
for i in [0, to_keep.size()):
  if to_keep[i] == 0:
    if extract_removed_indices_: removed.push_back(indices_[i])
  else:
    output.push_back(indices_[i])
```

本轮只隔离这一段，不改 searcher、不改 predicate、不改 production dispatch。

## 3. RVV 覆盖与 fallback

| 项目 | 结论 |
| --- | --- |
| 覆盖数据 | `std::vector<uint8_t> to_keep` + `pcl::Indices source_indices` |
| RVV 内容 | `vle8` 读 keep mask，`vle32` 读 source index，`vmsne` 生成 keep mask，`vcompress` 输出 kept / removed，`vcpop` 计数，`vse32` 写结果 |
| 保持标量 | `nearestKSearch`、`radiusSearch`、finite 检查、`negative_` predicate、生产 `RadiusOutlierRemoval` 入口 |
| fallback | 小规模 `<64`、非 RVV 编译、输入尺寸不匹配时使用标量 helper |
| 公开 API | 不改变 |
| FRM/FCSR | 不涉及显式舍入模式，不修改浮点环境 |

## 4. S0-S4 设计记录

FunctionCandidate：

- topic：`radius_outlier_removal`
- source file：`filters/include/pcl/filters/impl/radius_outlier_removal.hpp`
- queue section：filters 保留候选复筛 `6.2 暂缓 / 不单独实施（诊断路径记录）` 第 7 项
- mode：`bench-only-diagnostic`
- candidate fragment：`to_keep` 到 `indices` / `removed_indices_` 的尾段压缩
- main-cost risk：`nearestKSearch` / `radiusSearch` 主导真实入口

DiagnosticDesign / RVVPlan：

- `applyFilterIndicesTailStdReplay`：逐行复刻 `applyFilterIndices` 尾段循环，作为标量基准；
- `applyFilterIndicesTailRVVReplay`：对应同一尾段循环，用 RVV 替代 `to_keep` 压缩；
- `runApplyFilterIndicesDiagnostic`：模拟 `applyFilterIndices` 的 search-dominated 前段 + 尾段压缩，用来判断局部收益能否穿透 full diagnostic；
- `RadiusOutlierRemovalEntryDiagnostic`：test-only 子类，继承 `pcl::RadiusOutlierRemoval<PointT>`，提供 `applyFilterIndices(Indices&)` 同形诊断入口，通过 fake indices 和 explicit indices smoke 记录真实调用形状；
- 不修改生产源码，不新增生产 dispatch。

Source coverage：

| 项目 | 内容 |
| --- | --- |
| source file | `filters/include/pcl/filters/impl/radius_outlier_removal.hpp` |
| source function | `RadiusOutlierRemoval<PointT>::applyFilterIndices(Indices&)` |
| RVV 覆盖范围 | search / predicate 已填好 `to_keep` 后的尾段压缩循环 |
| RVV 操作 | 读取 `to_keep`、读取 source indices、生成 keep/removed mask、`vcompress` 保序写 kept/removed |
| 标量保留 | searcher 初始化、`nearestKSearch`、`radiusSearch`、finite 检查、`negative_` predicate、生产入口 |
| staging | 无复杂 staging；`vcompress` 后 kept/removed 顺序与标量扫描顺序一致 |

TestPlan：

- gtest 对拍 mostly-keep、half-keep shuffled、mostly-remove / no-removed；
- 小规模 fallback；
- full diagnostic checksum 对齐；
- 未修改生产 `RadiusOutlierRemoval<PointXYZ>` 仍可运行；
- test-only 子类对拍上游 fake indices 入口；
- test-only 子类对拍上游 explicit indices + duplicate + `negative=true` 入口；
- QEMU test/bench、asm、board test/bench 均收集。

## 5. 验证状态

| 项目 | 状态 |
| --- | --- |
| QEMU 专项测试 | `make -C test-rvv/filters/radius_outlier_removal run_test_compare` 通过，std/RVV 均 8 项通过 |
| QEMU bench | `make -C test-rvv/filters/radius_outlier_removal run_bench_compare` 通过 |
| bench 解析 | `output/qemu/analyze_bench_compare.log`、`output/board/analyze_bench_compare.log` 均可解析 |
| 反汇编 | `make -C test-rvv/filters/radius_outlier_removal dump_bench_rvv` 生成 `build/asm/riscv/bench_radius_outlier_removal_rvv.full.asm` 和 `.asm` 摘录 |
| 板卡验证 | 补充 entry smoke 后 `make -C test-rvv/filters/radius_outlier_removal run_board_test fetch_board_logs` 通过，`output/board/run_test.log` 显示 8 项通过；性能结论继续使用此前 `run_board_bench_compare` 日志 |

QEMU 只作为构建、checksum、日志格式和指令路径证据，不作为性能结论。

反汇编摘录中确认 `vle8.v`、`vmsne.vi`、`vcompress.vm`、`vcpop.m`、`vse32.v`、`vsetvli`。该二进制还包含其它库或编译器生成的 RVV 指令，不能把所有 RVV 指令都归因到本主题手写 helper。

## 6. 板卡结果

Milkv-Jupiter 结果：

| case | Std ms/iter | RVV ms/iter | speedup | 结论 |
| --- | ---: | ---: | ---: | --- |
| `radius_outlier_removal tail compress mostly-keep 64K` | 1.1860 | 1.2416 | 0.96x | tail-only 不成立 |
| `radius_outlier_removal tail compress half-keep shuffled 64K` | 1.3580 | 1.3671 | 0.99x | tail-only 持平 |
| `radius_outlier_removal tail compress mostly-remove 64K` | 1.0153 | 0.9934 | 1.02x | 弱收益 |
| `radius_outlier_removal tail compress mostly-keep 1M` | 17.2349 | 18.9424 | 0.91x | tail-only 退化 |
| `radius_outlier_removal tail compress half-keep shuffled 1M` | 20.1653 | 19.6219 | 1.03x | 弱收益 |
| `radius_outlier_removal tail compress no-removed 1M` | 10.2103 | 7.7148 | 1.32x | 局部 no-removed case 有收益，但不代表生产 |
| `radius_outlier_removal full diag light-search 64K` | 2.0406 | 1.9128 | 1.07x | 轻 search 模拟下弱收益 |
| `radius_outlier_removal full diag search-dominated 64K` | 10.1287 | 9.9310 | 1.02x | search 主导后收益被稀释 |
| `radius_outlier_removal full diag search-dominated shuffled 64K` | 83.8979 | 83.6845 | 1.00x | 持平 |
| `radius_outlier_removal full diag search-dominated 1M` | 163.3968 | 164.2549 | 0.99x | full diagnostic 不成立 |
| `radius_outlier_removal full diag search-dominated mostly-remove 1M` | 162.9816 | 164.2383 | 0.99x | full diagnostic 不成立 |

## 7. 当前接入判断

当前保持 bench-only 诊断，不接入生产路径。理由：

- 板卡 full diagnostic 在 search-dominated case 中基本为 `0.99x` 到 `1.02x`，没有稳定明显收益；
- tail-only 收益不稳定，mostly-keep 1M 退化到 `0.91x`，只有 no-removed 1M 达到 `1.32x`；
- 真实 `applyFilterIndices` 的主成本来自 `nearestKSearch` / `radiusSearch`，尾段压缩只能给出后处理成本上界；
- 生产接入需要新增分流、fallback、removed/no-removed 变体和维护文档，但证据不足以覆盖这些复杂度。

新增 `subclass-original` smoke 不改变该判断。它只说明如果后续要继续推进 production-shaped 诊断，应从真实公开入口生命周期出发，覆盖 fake indices、显式 subset、重复 index、`negative_`、removed indices 和 searcher 初始化；当前 RVV 证据仍停留在 tail replay 和 synthetic full diagnostic。

后续只有在真实 profile 证明 `to_keep` 压缩接近入口主成本，或 search 阶段已有独立 RVV/算法收益并让尾段成为瓶颈时，才应重新评估生产候选。
