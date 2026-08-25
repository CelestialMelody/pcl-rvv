# Organized Edge Detection RVV

## 当前状态

`features/include/pcl/features/impl/organized_edge_detection.hpp` 当前采用一条有界 RVV production path
（生产路径）：`OrganizedEdgeBase<PointT, PointLT>::extractEdges()` 在 `__RVV10__` 构建下，若
`PointT` 满足 `pcl::rvv::RVVXYZAoSFloatLayout<PointT>` 且 `PointLT` 为 `pcl::Label`，会先尝试
`pcl::detail::organizedEdgeDepthLabelsRVV()` 计算 depth label（深度标签）。gate（准入条件）不满足时，
或非 RVV 构建下，继续执行 `pcl::detail::organizedEdgeDepthLabelsStandard()` 标量 helper。

这份文档只描述已采纳的 depth label 生产行为。Phase 000 的 test-only diagnostic（测试专用诊断）、
Phase 010 的 production probe（生产探针）计划 / 结果、Phase 020 的 point-type expansion（点型扩展）证据和后续 scope expansion（范围扩展）候选，主归属在
`test-rvv/features/organized_edge_detection/doc/**`。

## 函数语义和标量路径

公开入口通过 `OrganizedEdgeBase::compute()` 进入 depth edge（深度边缘）流程：

1. `compute()` 按输入 organized cloud（有组织点云）的宽高初始化 `PointCloud<Label>`，把每个 label 置 0。
2. `extractEdges()` 遍历内部像素，不处理最外圈边界。
3. 对每个有限中心点读取 8 邻域 depth，若全部邻域有限，计算中心 depth 与邻域 absolute z（绝对 z）的差值，
   从最小 / 最大差值中选出绝对值更大的 dominant distance（主导距离），再按
   `th_depth_discon_ * abs(curr_depth)` 判断 occluding / occluded label bit（遮挡 / 被遮挡标签位）。
4. 若 8 邻域中存在 NaN / Inf，标量路径先统计 invalid neighbor（无效邻域）方向，再沿平均方向搜索
   `max_search_neighbors_` 范围内的下一个有限 depth；找到时按深度差分类，找不到时写 NaN boundary label。
5. `assignLabelIndices()` 线性扫描 labels，按 edge type bit 顺序把 index push 到各类 `PointIndices`。这一段
   是输出顺序语义的一部分，当前保持标量。

RGB Canny、normal Canny 和 RGB+normal 派生类会先调用 base depth path，因此可间接受益于 depth RVV helper；
但灰度图 / normal 图构造和 `pcl::Edge` Canny 处理没有纳入当前生产优化。

## 当前采用的优化方式

| 维度 | 当前状态 | 采用或暂缓原因 | 证据 | 边界 / 下一步 |
| --- | --- | --- | --- | --- |
| dispatch / fallback（分流 / 回退） | `extractEdges()` 先调用 RVV helper；返回 true 则结束，否则调用 Std helper。 | public API 不变，原 depth 标量主体被抽成清晰 fallback。 | `run_test_compare` Std/RVV 各 7/7 pass。 | 非 RVV 构建自然只有 Std helper。 |
| 输入布局 | RVV helper 使用 `RVVXYZAoSFloatLayout<PointT>`，当前只读取 z 字段；`PointLT` 收窄为 `pcl::Label`。 | 通过 traits gate（字段布局准入）避免对不兼容点型误走跨步 load。 | production direct correctness 和 board production bench 覆盖 `PointXYZ`、`PointXYZI`、`PointXYZRGB`、`PointXYZRGBNormal` + `Label`。 | 未测自定义点型和泛型 `PointLT` 不外推；`PointXYZRGBNormal` 收益按单独 case 报告。 |
| 全有限邻域主路径 | 每个 VL chunk 同时读取中心点和 8 邻域 z，向量化 finite mask、min/max、dominant distance、阈值比较和 label 写回。 | 原标量热点是规则 organized-grid 8 邻域扫描，适合跨步 load 和 mask 计算。 | production board mean `6.194x` / `5.521x`。 | 只覆盖内部像素；边界像素仍保持原 labels。 |
| invalid neighbor | chunk 中任一 lane 有 invalid neighbor 时，先写全有限 lanes 的 RVV 结果，再逐 lane 调同一 scalar pixel helper 修正。 | NaN boundary 搜索有方向累计和变长 search，保留标量更稳。 | NaN boundary production case mean `3.016x`，checksum match。 | invalid-heavy 真实 workload 若收益变弱，需另做 profile 或消融。 |
| label index 收集 | `assignLabelIndices()` 保持标量。 | 顺序 push 是公开输出语义，当前 depth RVV 收益已足够强。 | correctness 覆盖 label index 顺序；未做收集性能优化。 | 只有 profile 显示它成为主成本时再恢复 `assign-label-indices-ablation`。 |
| RGB / normal 派生入口 | 仅复用 base depth path；派生 Canny 前处理保持标量。 | Canny helper 可能主导成本，不能从 depth 证据外推。 | 当前没有 RGB / normal production bench。 | 另开 RGB / normal 派生入口诊断。 |

## VL Chunk 流程

RVV depth helper 对每一行内部像素按 VL chunk（可变向量长度分块）处理：

```text
for row in [1, height - 2]:
  for chunk over columns [1, width - 2]:
    center_z = strided_load z(row, col..col+vl)
    center_abs = abs(center_z)
    all_finite = isfinite(center_z)

    min_dist = +max_float
    max_dist = -max_float
    for each of 8 neighbor directions:
      neighbor_z = strided_load z(row + dy, col + dx)
      all_finite &= isfinite(neighbor_z)
      dist = center_abs - abs(neighbor_z)
      min_dist = min(min_dist, dist)
      max_dist = max(max_dist, dist)

    dominant = abs(min_dist) > abs(max_dist) ? min_dist : max_dist
    discontinuity = all_finite && abs(dominant) > th_depth_discon * center_abs
    out_label = occluded if dominant > 0, occluding if dominant < 0
    labels |= out_label

    if any lane is not all_finite:
      recompute that lane with organizedEdgeDepthLabelAtStandard()
```

`vsetvl` 让最后一个 chunk 自然处理 tail（尾段），不要求图像宽度是向量长度倍数。label 写回使用
`vlse32` / `vsse32` 以 `sizeof(PointLT)` 为 stride（跨步），保持 `pcl::Label` 结构数组布局。

## 数值算例

假设某个中心点 `z=10.0`，阈值 `th_depth_discon=0.02`，8 邻域 absolute z 的差值里最小值为 `-0.5`，
最大值为 `0.3`。主导距离取绝对值更大的 `-0.5`，阈值是 `0.02 * 10.0 = 0.2`。因为 `abs(-0.5) > 0.2`
且主导距离为负，若检测类型包含 `EDGELABEL_OCCLUDING`，该像素写入 occluding label bit。

如果同一 chunk 里另一个 lane 的邻域包含 NaN，RVV helper 不把该 lane 的向量结果当最终语义，而是调用
`organizedEdgeDepthLabelAtStandard()` 重新执行 invalid-neighbor 方向搜索。这样全有限 lane 继续享受批处理收益，
NaN boundary lane 保持原标量行为。

## Fallback 矩阵

| 条件 | 行为 | 维护边界 |
| --- | --- | --- |
| 非 `__RVV10__` 构建 | RVV include 和 intrinsic path 不参与编译，执行 Std helper。 | 非 RVV 行为与原语义一致。 |
| `PointT` 不满足 `RVVXYZAoSFloatLayout` | RVV helper 返回 false，`extractEdges()` 调 Std helper。 | 自定义点型、非 float z 或非标准 AoS 布局不误命中。 |
| `PointLT != pcl::Label` | RVV helper 返回 false，执行 Std helper。 | 泛型 label field gate 尚未实现。 |
| 不检测 depth / NaN edge bit | RVV helper 返回 true 且不写 label，与原 depth path 早退语义一致。 | 派生 RGB / normal 入口可继续执行各自 Canny path。 |
| `width < 3` 或 `height < 3` | RVV helper 返回 true，labels 保持初始化值。 | 原标量内部循环同样没有可处理像素。 |
| 中心或邻域 NaN / Inf | 全有限主路径不作为最终结果，逐 lane 调标量 pixel helper 修正。 | NaN boundary search、找不到对应点时写 label 的语义保持标量。 |
| RGB / normal Canny | 不属于当前 RVV helper；仍执行原派生类实现。 | depth path 证据不能外推到 Canny 前处理。 |

## Bench 与证据

证据主归属：

- Phase 010 result：`test-rvv/features/organized_edge_detection/doc/phases/010-production-depth-label-probe/result.zh.md`
- evaluation：`test-rvv/features/organized_edge_detection/doc/organized_edge_detection-evaluation.zh.md`
- production repeated summary：`test-rvv/features/organized_edge_detection/log/board/production-repeated-summary.md`
- Evidence Doctor：`test-rvv/features/organized_edge_detection/log/board/production-evidence_doctor.md`
- Phase 020 point-type summary：`test-rvv/features/organized_edge_detection/log/board/point-type-repeated-summary.md`
- Phase 020 point-type Evidence Doctor：`test-rvv/features/organized_edge_detection/log/board/point-type-evidence_doctor.md`
- evidence registry：`test-rvv/features/organized_edge_detection/log/evidence_registry.json`

性能结论只使用接入 production 后的 repeated board（重复板卡测试）结果。QEMU timing（仿真器计时）不作为性能证据。

| case | 入口 / 计时边界 | evidence role（证据角色） | mean speedup | median speedup | 证明点 | 不能证明 |
| --- | --- | --- | ---: | ---: | --- | --- |
| `prod_depth_finite_320x240` | public `OrganizedEdgeBase<PointXYZ, Label>::compute()` | `production-public` | 6.194x | 6.192x | 常规 organized depth grid 下真实公开入口强正向。 | RGB / normal 派生 Canny、其它点型。 |
| `prod_depth_finite_641x481_tail` | public `compute()`，非整齐宽高 | `production-public` | 5.521x | 5.544x | tail 规模下 `vsetvl` 分块仍强正向。 | 所有图像尺寸或真实 workload 分布。 |
| `prod_depth_nan_boundary_320x240` | public `compute()`，含 NaN boundary | `production-public` | 3.016x | 3.013x | invalid neighbor lane fallback 后仍有稳定收益。 | invalid-heavy workload 中 label index 或 search 成本占比。 |
| `prod_depth_pointxyzi_finite_320x240` | public `OrganizedEdgeBase<PointXYZI, Label>::compute()` | `production-public` | 5.327x | 5.380x | 常见 intensity 点型的 z-only depth path 保持强正向。 | 泛型 `PointLT`、其它自定义点型。 |
| `prod_depth_pointxyzrgb_finite_320x240` | public `OrganizedEdgeBase<PointXYZRGB, Label>::compute()` | `production-public` | 5.437x | 5.438x | RGB 点型只经 base depth path 时保持强正向。 | RGB Canny 灰度前处理。 |
| `prod_depth_pointxyzrgbnormal_finite_320x240` | public `OrganizedEdgeBase<PointXYZRGBNormal, Label>::compute()` | `production-public` | 4.158x | 4.216x | 更大 stride 点型仍保持 positive。 | 该 case 是组内较低收益，不能继承其它点型的更高 speedup。 |
| `prod_depth_pointxyzrgb_nan_boundary_320x240` | public `OrganizedEdgeBase<PointXYZRGB, Label>::compute()`，含 NaN boundary | `production-public` | 2.822x | 2.828x | invalid neighbor fallback 与 RGB 点型组合后仍正向。 | invalid-heavy 真实 workload、RGB Canny。 |

Phase 010 Evidence Doctor（证据体检）结果为 `Errors=0, Warnings=0, Suggestions=3`。Phase 020 point-type
Doctor 为 `Errors=0, Warnings=1, Suggestions=4`；Warning 是 `PointXYZRGBNormal` group outlier（组内离群），
median `4.216x` 低于同组 median 约 21.6%，但该 case 的 min B/A 仍为 `3.952x`。因此当前结论是该具体点型
positive，但不把其它点型的更高收益外推到所有大 stride 点型。Suggestions 均为缺少 taskset、governor、freq、
temperature 环境 metadata（元数据）；若后续扩展点型或真实数据集出现长尾或方向反转，应先补齐环境字段和
binary identity（二进制身份）后重跑。

## 正确性与高效性证据链

| 层级 | 证据 | 结论 |
| --- | --- | --- |
| correctness（正确性） | `make -C test-rvv/features/organized_edge_detection run_test_compare`：Std/RVV 各 7/7 pass。 | test helper 和真实 production `compute()` 的 label bits、NaN boundary 和 label index 顺序与标量参考一致；Phase 020 覆盖三种新增点型。 |
| production direct（真实生产入口） | `ProductionComputeMatchesScalarReference` 调用 `OrganizedEdgeBase<PointXYZ, Label>::compute()`，Phase 020 typed tests 调用 `PointXYZI`、`PointXYZRGB`、`PointXYZRGBNormal`。 | 公开入口真实命中生产补丁，不只是 test-only helper。 |
| asm attribution（反汇编归属） | `make -C test-rvv/features/organized_edge_detection check_production_rvv_asm` 通过；production bench asm 中 `organizedEdgeDepthLabelsRVV` 含 `vle32.v`、`vfsub.vv`、`vfmin.vv`、`vfmax.vv`、`vse32.v`。 | 关键 RVV 指令归属到生产 helper。 |
| performance（性能） | Milkv-Jupiter 5-run production summary：Phase 010 三组 mean 为 6.194x、5.521x、3.016x；Phase 020 四组 point-type mean 为 5.327x、5.437x、4.158x、2.822x，checksum match。 | 当前有界生产补丁值得保留；已测常见点型扩展也保持 positive。 |
| boundary（边界） | fallback matrix、optimization matrix、Phase 010 result 和 Phase 020 result 均记录未覆盖范围。 | 结论只覆盖当前 depth label production path 与已测点型；派生 Canny、自定义点型和泛型 `PointLT` 需要独立证据。 |

## Traceability Map

| 符号 / 文件 | 层级 | 作用 | 调用者 / 上游入口 | 被调用者 / 下游消费者 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- | --- | --- |
| `OrganizedEdgeBase::compute()` | production public entry | 初始化 labels，调用 depth extraction，再收集 label indices。 | PCL feature caller | `extractEdges()`、`assignLabelIndices()` | production direct public boundary | `features/include/pcl/features/impl/organized_edge_detection.hpp` |
| `OrganizedEdgeBase::extractEdges()` | production dispatch / fallback | 先尝试 RVV helper，失败时调用 Std helper。 | base / RGB / normal / RGB+normal `compute()` | depth labels | adopted dispatch boundary | `features/include/pcl/features/impl/organized_edge_detection.hpp` |
| `organizedEdgeDepthLabelsStandard()` | production Std helper | 承载原 depth label 标量主体。 | `extractEdges()` fallback | labels | fallback semantic baseline | `features/include/pcl/features/impl/organized_edge_detection.hpp` |
| `organizedEdgeDepthLabelsRVV()` | production RVV helper | RVV 计算全有限邻域 depth label，并对 invalid lane 标量修正。 | `extractEdges()` | labels | adopted production implementation | `features/include/pcl/features/impl/organized_edge_detection.hpp` |
| `organizedEdgeDepthLabelAtStandard()` | scalar pixel helper | 单像素 depth / NaN boundary 分类。 | Std helper、RVV invalid-lane fallback | labels | scalar equivalence boundary | `features/include/pcl/features/impl/organized_edge_detection.hpp` |
| `src/test_organized_edge_detection.cpp` | correctness tests | 对拍 depth step、NaN boundary、edge type bit 和 production compute。 | `run_test_compare` | gtest | correctness gate | `test-rvv/features/organized_edge_detection/src/test_organized_edge_detection.cpp` |
| `src/bench_organized_edge_detection_production.cpp` | production bench wrapper | 构造 production-public board cases 并调用真实 `compute()`，包含 Phase 020 点型 case-filter。 | board repeated target | summary / manifest | board performance | `test-rvv/features/organized_edge_detection/src/bench_organized_edge_detection_production.cpp` |
| `generate_organized_edge_detection_evidence_manifest.py` | analysis script | 从 repeated run 生成 production summary 和 manifest。 | board repeated logs | Evidence Doctor / registry | evidence validation | `test-rvv/features/organized_edge_detection/script/generate_organized_edge_detection_evidence_manifest.py` |
| Phase 010 result | phase closeout | PI1-PI5 事实、fallback matrix、EvidenceDecision 和继续 / 停止判断。 | reviewer / worker | evaluation / this doc | recovery pointer | `test-rvv/features/organized_edge_detection/doc/phases/010-production-depth-label-probe/result.zh.md` |
| Phase 020 result | phase closeout | 点型扩展 evidence、Doctor warning 解释和继续 / 停止判断。 | reviewer / worker | evaluation / this doc | point-type coverage pointer | `test-rvv/features/organized_edge_detection/doc/phases/020-point-type-expansion/result.zh.md` |
| evaluation | decision audit | 函数级评估、Traceability Map 和生产接入判断主归属。 | reviewer / worker | this doc | production decision audit | `test-rvv/features/organized_edge_detection/doc/organized_edge_detection-evaluation.zh.md` |

## 生产 Closeout

| area | 当前状态 |
| --- | --- |
| production file | `features/include/pcl/features/impl/organized_edge_detection.hpp` |
| public API | 不变。 |
| RVV compile gate | `__RVV10__`。 |
| layout / type gate | `pcl::rvv::RVVXYZAoSFloatLayout<PointT>` 和 `PointLT == pcl::Label`。 |
| adopted helper | `pcl::detail::organizedEdgeDepthLabelsRVV()` for depth label path。 |
| fallback | RVV helper 返回 false 或非 RVV 构建时执行 `organizedEdgeDepthLabelsStandard()`。 |
| test scope | `PointXYZ`、`PointXYZI`、`PointXYZRGB`、`PointXYZRGBNormal -> Label`, organized depth grid, finite / tail / NaN boundary synthetic scenes。 |
| evidence | QEMU Std/RVV 各 7/7、production asm attribution、Phase 010 / Phase 020 board repeated、Evidence Doctor 0/0/3 与 0/1/4。 |
| rollback boundary | 删除 RVV include、RVV helper 和 `extractEdges()` 中的短路即可回到原标量行为；不涉及公开 API。 |

## 后续方向

当前 depth production path 已采纳，Phase 020 已补齐常见点型的 production-public evidence。后续方向都属于更宽
scope expansion（范围扩展）或独立诊断，不阻塞当前补丁：

- RGB / normal Canny 派生入口：先做 component bench 和 correctness；不能从 depth label 证据外推。
- `assignLabelIndices` 消融：只有 production profile 显示 label index 收集成为主成本时恢复，且必须证明输出顺序不变。
- 泛型 `PointLT` 或自定义点型扩展：需要新的 traits / label field gate 设计、fallback 测试和同边界板卡证据；当前没有性能或用户需求证据，不建议继续默认推进。
- board metadata：若后续性能数据出现长尾或反转，优先补 taskset、governor、freq、temperature 和 binary identity。
