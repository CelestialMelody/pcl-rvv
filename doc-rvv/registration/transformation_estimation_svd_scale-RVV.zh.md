# transformation_estimation_svd_scale RVV 生产接入记录

## 当前状态

收尾判断：Phase 075 已关闭同范围优化，当前没有值得继续推进的 positive same-scope route。后续若要再做，只能另起新 scope，重新定义输入分布、family selection 或 board budget。

| 结论项 | 当前状态 | 读者应如何理解 |
| --- | --- | --- |
| 已采纳主线 | `direct-fused-scale-accum`、row-source 扩展、`affine-index-fast-path`、`Scalar=double` ordered / row-source / generic 分支、custom layout double 取样、`matrix-local-scale-simplification` | 这些分支已经进入长期生产事实，文档只负责维护它们的当前边界。 |
| 已回滚分支 | `correspondence-sorted-copy-scalar-double-production-probe` | Phase 072 的 public-positive 只说明快于 public scalar fallback；Phase 073 证明它慢于既有 D64 gather，Phase 075 已回滚。 |
| 已关闭负向路线 | staged-selected-cloud、dual-indexed target-sorted、dual-indexed 256K source-sorted-copy | 这些路线已被 negative / unstable 证据收束，不应作为当前继续优化方向。 |
| 当前文档角色 | maintenance-only production doc | 只记录当前采用的 production 行为、fallback 边界、证据链和不再覆盖的范围。 |

`transformation_estimation_svd_scale` 当前采用多条用户确认过的 RVV production behavior（生产行为），并额外采纳一个 production helper simplification（生产 helper 简化）。下表先列当前生产事实；历史候选、回滚和负向路线放在后续表格中。

### 当前生产行为一览

| 状态 | 优化族 | 入口 / 行来源 | 覆盖边界 |
| --- | --- | --- | --- |
| adopted-by-user | `direct-fused-scale-accum` | ordered-cloud-pair | traits-gated xyz AoS，`Scalar=float`。 |
| adopted-by-user | row-source direct fused | source-indexed、dual-indexed、correspondence | traits-gated xyz AoS，`Scalar=float`。 |
| adopted-by-user | correspondence sorted-copy | correspondence | size >= 64K，shuffle-like disorder，`Scalar=float`。 |
| adopted-by-user | `affine-index-fast-path` | source-indexed、dual-indexed、correspondence | step=1 contiguous indices / correspondences，traits-gated xyz AoS，`Scalar=float`。 |
| adopted-by-user | ordered exact double | ordered-cloud-pair | exact `PointXYZ -> PointXYZ`，`Scalar=double`，dense，`nr_points >= 16`。 |
| adopted-by-user | row-source exact double | source-indexed、dual-indexed、correspondence | exact `PointXYZ -> PointXYZ`，`Scalar=double`，dense，`nr_points >= 16`。 |
| adopted-by-user | ordered generic double | ordered-cloud-pair | common PCL xyz AoS whitelist，`Scalar=double`，dense，`nr_points >= 16`。 |
| adopted-by-user | row-source generic double | source-indexed、dual-indexed、correspondence | common PCL xyz AoS whitelist，`Scalar=double`，dense，64K。 |
| adopted-by-user | custom layout double scout | ordered + 三类 row-source | 一个测试本地 registered custom xyz AoS sample，`Scalar=double`，dense，64K。 |
| adopted-by-user | more custom layout double sampling | ordered + 三类 row-source | compact、huge-padding、aligned 三组测试本地 custom xyz AoS samples，`Scalar=double`，dense，64K。 |
| adopted-by-user | `matrix-local-scale-simplification` | `getTransformationFromCorrelation` helper | 使用 `trace(R * H)` 简化后段 scale helper，不新增 RVV intrinsic family。 |

### Phase 069-071 接入后收益

Phase 074 根据用户确认，把 Phase 069、Phase 070 和 Phase 071 写入 adopted 集合。采纳依据只使用接入后的 board repeated 数据。

| phase | 覆盖范围 | board 结论 | Evidence Doctor | 解释边界 |
| --- | --- | --- | --- | --- |
| Phase 069 | row-source common PCL xyz AoS `Scalar=double` | 9 个 case 全 positive，median B/A `11.309x` 到 `17.433x`。 | `0/4/0` | 只覆盖 common PCL whitelist、dense、64K、合法 index / correspondence。 |
| Phase 070 | `LocalSVDScalePaddedXYZSource -> LocalSVDScaleWideXYZTarget` custom layout sample | 4 个 public path 全 positive，median B/A `9.966x` 到 `27.497x`。 | `0/1/0` | 只覆盖该测试本地 custom layout sample。 |
| Phase 071 | compact、huge-padding、aligned 三组 custom layout sample | 12 个 public path 全 positive，median B/A `2.766x` 到 `29.087x`。 | `0/8/0` | 只作为 Phase 070 的采样增强，不外推到全部 custom layout double。 |

### sorted-copy double 回滚结论

Phase 075 根据同一边界选择更快实现的规则，回滚 Phase 072 的 correspondence sorted-copy `Scalar=double` 分流。

| phase | 证据角色 | board 结果 | 当前决策 |
| --- | --- | --- | --- |
| Phase 072 | public Std/RVV production probe | 64K / 256K median B/A `5.727x` / `5.408x`，Doctor `0/0/0`。 | 只保留为 historical public-positive evidence。 |
| Phase 073 | 同边界 RVV-vs-RVV family selection | sorted-copy double 相对 D64 gather 的 64K / 256K median B/A 为 `0.283x` / `0.431x`，Doctor `2/1/0`。 | 不支持 clean adoption。 |
| Phase 075 | rollback closeout | rollback 后 correctness 仍为 Std/RVV 38/38 passed。 | 当前 `Scalar=double` shuffled correspondence 在 contiguous fast path 不命中时继续使用 D64 gather。 |

### 补充证据边界

这些 phase 扩大或解释证据边界，但不新增 production behavior。

| phase | 作用 | 当前解释 |
| --- | --- | --- |
| Phase 041 | ordered representative generic point types | 补 `PointXYZI` / `PointXYZRGB` 代表点型的 ordered public board evidence。 |
| Phase 044 | row-source representative generic point types | 补 row-source representative generic xyz AoS evidence。 |
| Phase 051 / 052 | more common PCL xyz AoS ordered evidence | 补 correctness、QEMU smoke 和 board repeated。 |
| Phase 053 / 054 | more common PCL xyz AoS row-source evidence | 补代表组合和全交叉 evidence。 |
| Phase 055 / 056 / 057 / 059 | custom layout、padding、alignment sampling | 扩大采样边界；不能写成全部 custom layout clean positive。 |
| Phase 045 | row-source locality / order profile | 解释 row-source 收益对 locality 和 order pattern 敏感。 |

### 已关闭的负向路线

| phase | 路线 | 结论 |
| --- | --- | --- |
| Phase 048 | staged-selected-cloud | board detail A/B 为 negative，不进入 production。 |
| Phase 049 | dual-indexed target-sorted | 64K negative、256K mixed / weak-positive，不进入 production。 |
| Phase 058 | dual-indexed 256K source-sorted-copy | stability 复核为 rejected / unstable，不进入 production probe。 |

### 仍未覆盖的范围

当前结论不表示以下范围已经获得同等生产证据：

- 全部 custom layout double 或任意自定义点型全集。
- 非 dense 输入、小规模输入、退化 source variance。
- 非法 index / correspondence。
- 异常 layout、padding、alignment。
- dual-indexed sorted-copy 和 sorted-copy double family selection。
- stride / reverse / shuffle affine fast path。
- 任意 index 分布。

## 生产入口和 gate

当前已采纳的 production dispatch 位于：

```text
registration/include/pcl/registration/transformation_estimation_svd_scale.h
registration/include/pcl/registration/impl/transformation_estimation_svd_scale.hpp
```

ordered-cloud-pair 公开入口只有以下条件同时满足时才尝试 RVV：

- RVV 构建，即 `__RVV10__` 可用；
- ordered-cloud-pair public overload；
- `Scalar == float`；
- `PointSource` 满足 `pcl::rvv::RVVXYZAoSFloatLayout<PointSource>`；
- `PointTarget` 满足 `pcl::rvv::RVVXYZAoSFloatLayout<PointTarget>`；
- source / target 点数相等且不少于 16；
- 两侧 `is_dense == true`；
- RVV 累加后 source variance 非退化，scale 解为 finite（有限值）。

任一 gate 失败时，公开入口显式调用父类 overload，继续使用原有 scale 标量路径。

ordered-cloud-pair 的 `Scalar=double` 分支是独立窄 gate。Phase 066 最初只采纳 exact `PointXYZ -> PointXYZ`，Phase 068 已把 adopted 证据扩到 common PCL xyz AoS whitelist；Phase 070 的 adopted layout-gated patch 进一步允许满足 `RVVXYZAoSFloatLayout` 的 custom layout sample 命中。只有以下条件同时满足时才尝试 f64 widened RVV accumulation（双精度扩宽 RVV 累加）：

- RVV 构建，即 `__RVV10__` 可用；
- ordered-cloud-pair public overload；
- `Scalar == double`；
- `PointSource` 和 `PointTarget` 同时满足 `RVVXYZAoSFloatLayout`；已采纳证据覆盖 common PCL double whitelist，Phase 070 / 071 adopted boundaries 只额外证明四组测试本地 custom layout samples；
- source / target 点数相等且不少于 16；
- 两侧 `is_dense == true`；
- RVV f64 累加后 source variance 非退化，scale 解为 finite。

该 ordered double branch 的已采纳结论仍是 common PCL whitelist + traits-gated 分流，不是任意用户点型泛型结论。Phase 070 / 071 adopted patch 让四个 custom layout samples 有正向证据，但这只覆盖对应采样边界，不能把全部 custom layout double 写成 adopted；小规模或 non-dense 输入继续 fallback。

Phase 043 row-source patch 在 source-indexed、dual-indexed 和 correspondence overload 中使用同一 scale 累加 / 求解思路，且用户已确认采纳。`Scalar=float` row-source RVV 路径额外要求 index / correspondence 合法、`nr_points >= 16`、两侧 dense 和 traits-gated xyz AoS layout；否则回父类对应 overload。

Phase 067 row-source `Scalar=double` 分支是独立窄 gate。

它只在下面条件同时满足时才尝试 row-source f64 widened RVV accumulation：

- RVV 构建。
- `Scalar == double`。
- exact `PointSource == pcl::PointXYZ`。
- exact `PointTarget == pcl::PointXYZ`。
- 三类 row-source public overload。
- 合法 index / correspondence。
- dense 输入。
- `nr_points >= 16`。

source-indexed、dual-indexed 和 correspondence 都先检测 step=1 contiguous slice。命中时复用 contiguous offset f64 fast path。未命中但 gather gate 合法时走 f64 widened gather。非 exact `PointXYZ`、custom layout double、小规模、non-dense 或非法 index / correspondence 都继续父类 fallback。

Phase 069 row-source generic `Scalar=double` 分支复用 Phase 068 的 common PCL xyz AoS whitelist 和 `RVVXYZAoSFloatLayout` gate，并扩到 source-indexed、dual-indexed 和 correspondence public overload。

当前已由 Phase 074 根据用户确认收口为 adopted-by-user。Phase 070 adopted patch 进一步侦察 layout-gated custom double sample，并在 ordered 与三类 row-source 上得到 positive evidence。Phase 071 又补 compact、huge-padding 和 aligned 三组 custom layout 的同边界取样增强。

Phase 072 进一步侦察 correspondence sorted-copy double，并得到 public Std/RVV positive evidence。Phase 073 的同边界 RVV-vs-RVV detail A/B against D64 gather 为 negative，因此 Phase 075 回滚 sorted-copy double 生产分流。

当前 `Scalar=double` correspondence 分支在 contiguous fast path 不命中后直接使用 D64 gather RVV family。小规模、non-dense、非法 index / correspondence、更多 custom layout、任意自定义点型全集和 sorted-copy double family selection 继续 fallback / 未覆盖。

Phase 061 contiguous affine index fast path 只在上述 row-source RVV gate 通过后继续尝试。source-indexed 路径要求 source indices 是 step=1 contiguous slice，target 仍按 selected target 的 0 起点顺序读取；dual-indexed 路径要求 source / target 两组 indices 都是 step=1 contiguous slice 且 count 相同；correspondence 路径要求 query 和 match 都是 step=1 contiguous slice。命中时复用 contiguous offset RVV accumulation，避免 gather；未命中时保持 Phase 043 的 gather RVV path、Phase 047 的 correspondence sorted-copy 分支或父类 fallback。

Phase 047 correspondence sorted-copy 分支只在 correspondence overload 内部生效。它先通过普通 correspondence RVV gate，再要求 `correspondences.size() >= 65536` 且 query index 顺序呈 shuffle-like disorder（类似洗牌的非规则乱序）。满足时复制 correspondence 并按 query / match / distance 排序，再复用 correspondence RVV accumulation；不满足时保持 Phase 043 的普通 gather RVV path。

## 标量路径

标量 scale 路径来自 `TransformationEstimationSVD` 的 `use_umeyama_ == false` 分支：

```text
public overload
  -> ConstCloudIterator
  -> compute3DCentroid(source/target)
  -> demeanPointCloud(source/target)
  -> getTransformationFromCorrelation
       -> H = source_demean * target_demean^T
       -> Eigen 3x3 JacobiSVD
       -> R = V * U^T with determinant sign fix
       -> sum_ss = Σ ||source_demean||^2
       -> sum_tt = trace(R * H)
       -> scale = sum_tt / sum_ss
       -> transform = [scale * R, centroid_tgt - scale * R * centroid_src]
```

这个 fallback path（回退路径）仍会 materialize（物化）source / target 的 demean 动态矩阵，但 Phase 050 已采纳的 helper 简化不再构造 `R4 * cloud_src_demean` 临时矩阵，而是直接复用当前 helper 已有的 `H` 和 `R` 计算 `trace(R * H)`。

## 当前采用的优化方式

已采纳的 `direct-fused-scale-accum` 直接从 ordered source / target 点对累加 scale 所需统计量，避免生成 demean 动态矩阵和后段 rotated cloud 临时矩阵：

| 维度 | 当前状态 | 采用或暂缓原因 | 证据 | 边界 / 下一步 |
| --- | --- | --- | --- | --- |
| ordered direct fused accumulation | adopted | 该路径覆盖 scale public ordered 主成本，production direct board repeated 为强正向。 | Phase 010 / 030、QEMU correctness、ASM、board production summary。 | 只覆盖 ordered / `Scalar=float` / traits-gated xyz AoS。 |
| Eigen 3x3 SVD 和输出构造 | scalar tail retained | 主成本不在 3x3 solver；保留标量能降低数值和维护风险。 | correctness 27 tests；production ASM 显示 RVV 归属于累加段。 | 若未来需要 solver 级优化，另开数值预算。 |
| matrix-local-scale-simplification | adopted-by-user | Phase 020 / 042 证明局部公式等价且 weak-positive；Phase 050 已把它收口为 production helper simplification。 | `MatrixLocalScaleSimplificationMatchesLegacyPath`；matrix-local board summary / doctor；current 23-test correctness。 | 这是标量后段 helper 简化，不是新的 RVV intrinsic family；不扩大 dispatch、row-source、点型或 `Scalar` 范围。 |
| generic point type expansion | representative board positive + Phase 052 / 054 more-generic evidence positive | 当前 gate 已是 traits-gated xyz AoS；Phase 041 补齐 `PointXYZI` / `PointXYZRGB` 代表点型 public board evidence，Phase 051 又补 `PointXYZRGBA`、`PointXYZL`、`PointNormal`、`PointWithRange` 和 `PointWithViewpoint` 相关 common PCL xyz AoS 组合的 QEMU correctness / smoke，Phase 052 补这些组合的 64K board repeated，Phase 054 补这些组合在三类 row source 下的全交叉 board evidence。 | QEMU correctness；generic public board summary / doctor；Phase 051 more-generic public QEMU smoke Doctor；Phase 052 more-generic public board summary / doctor；Phase 054 row-source all-more-generic QEMU / board summary / doctor。 | Phase 054 只覆盖 5 个具体 common PCL 点型组合 × 三类 row source，不外推到全部自定义 xyz AoS、异常 layout / padding 或 `Scalar=double`。 |
| indexed / correspondence row source | adopted-by-user | Phase 043 已补 source-indexed、dual-indexed 和 correspondence public patch，board repeated 全部 positive，且用户已确认采纳。Phase 045 又解释 row-source 在 contiguous / stride / reverse / shuffle 下的 locality sensitivity。Phase 053 / 054 补更多常见点型 row-source evidence。 | Phase 043 / 044 / 045 / 053 / 054 result、row-source QEMU / board summary / doctor。 | Phase 044 已补代表泛型点型；Phase 054 已补常见 PCL 点型全交叉；shuffle 改善已转入独立 candidate。 |
| contiguous affine index fast path | adopted-by-user | Phase 060 先用同一 binary RVV-vs-RVV detail A/B 证明 contiguous offset accumulation 优于 current contiguous gather；Phase 061 再接入真实 public overload 并计入 contiguous 检测成本；Phase 062 根据用户确认收口为 adopted production branch。 | `AffineIndexFastPathCandidateMatchesReference`、`AffineIndexFastPathPublicProbeMatchesReference`；QEMU production probe Doctor `0/0/0`；board production probe 6/6 positive，median B/A `10.115x` 到 `14.021x`，Doctor `0/0/0`。 | 只覆盖 step=1 contiguous source indices、dual source/target indices 和 correspondence query/match；不外推到 stride / reverse / shuffle、非法 index / correspondence、custom layout 全集或 `Scalar=double`。 |
| correspondence sorted-copy production branch | adopted-by-user | Phase 047 把 Phase 046 的 correspondence 64K / 256K positive 子边界接入真实 correspondence public overload，并由用户确认采纳。 | current QEMU 23 tests；QEMU probe Doctor `0/0/0`；board probe median B/A `8.128x` / `3.929x` / `3.618x`，Doctor `0/1/0`。 | 只覆盖 correspondence、size >= 64K、shuffle-like disorder；不外推到 dual-indexed、小规模、规则顺序或 `double`。 |
| custom layout evidence expansion | sampled positive with warnings | Phase 055 / 056 / 057 / 059 补了测试本地 registered custom xyz AoS layout、padding sensitivity 和 alignment sensitivity 采样。 | Phase 059 当前 alignment 采样 6/6 board case positive，median B/A `1.636x` 到 `2.309x`，Doctor `Errors=0`、`Warnings=3`。 | 这是已采纳 traits-gated public path 的证据扩展，不新增 dispatch；不外推到全部 custom layout、packed unaligned float、异常 alignment 全集或 `Scalar=double`。 |
| staged-selected-cloud row-source mitigation | rejected diagnostic | Phase 048 的 selected-cloud copy + ordered public path 在 6 个 shuffled row-source case 全部 negative。 | board detail A/B median B/A：correspondence `0.711x` / `0.482x` / `0.563x`，dual-indexed `0.773x` / `0.776x` / `0.958x`；Doctor `Errors=6`、`Warnings=9`。 | 不进入 production；只作为 topic-local 负向诊断记录。 |
| dual-indexed target-sorted row-source mitigation | rejected diagnostic | Phase 049 的 target-sorted copy 在 dual-indexed 64K negative、256K weak-positive，overall negative。 | board detail A/B median B/A：64K `0.897x`、256K `1.170x`；Doctor `Errors=1`、`Warnings=1`。 | 不进入 production；只作为 topic-local 负向诊断记录。 |
| dual-indexed source-sorted-copy stability | rejected / unstable diagnostic | Phase 058 对 Phase 046 残留 dual-indexed 256K weak-positive 线索做 10-run 稳定性复核，median B/A 仅贴近 1 且退化频率高。 | board detail A/B B/A：`0.979, 1.087, 1.119, 1.167, 0.957, 0.804, 1.231, 0.902, 0.946, 1.041`，median `1.010x`，5/10 run 低于 1；Doctor `Errors=1`、`Warnings=1`、`Suggestions=1`。 | 不进入 production probe；Phase 047 correspondence sorted-copy branch 不受影响。 |
| `Scalar=double` ordered production branch | adopted-by-user | Phase 063/064 先完成 double diagnostic correctness 和 board diagnostic；Phase 065 接入真实 public ordered overload；Phase 066 根据用户确认收口为 adopted。 | Phase 065 31-test correctness；QEMU scalar-double production probe Doctor `0/0/0`；board median B/A `33.792x`，Doctor `0/0/0`。 | 只覆盖 exact `PointXYZ -> PointXYZ` / ordered / dense / `nr_points >= 16`；generic double 和 custom layout double 另开 phase。 |
| `Scalar=double` row-source production branch | adopted-by-user | Phase 067 接入真实 source-indexed、dual-indexed 和 correspondence public overload；用户确认后收口为 adopted。 | Phase 067 31-test correctness；QEMU row-source scalar-double production probe Doctor `0/0/0`；board median B/A `20.201x` / `14.800x` / `13.474x`，Doctor `0/1/0`。 | 只覆盖 exact `PointXYZ -> PointXYZ` / 三类 row-source / dense / `nr_points >= 16`；generic double、custom layout double、sorted-copy double 和非法 index / correspondence 另开 phase。 |
| `Scalar=double` ordered generic production branch | adopted-by-user | Phase 068 接入真实 ordered public overload，把 double branch 扩到 common PCL xyz AoS whitelist；用户确认后收口为 adopted。 | Phase 068 31-test correctness；QEMU generic ordered double probe Doctor `0/0/0`；board 5 个代表组合全 positive，median B/A `24.111x` 到 `32.493x`，Doctor `0/1/0`。 | 只覆盖 ordered common PCL xyz AoS whitelist / dense / `nr_points >= 16`；row-source generic double、custom layout double、sorted-copy double 和任意自定义点型全集另开 phase。 |
| `Scalar=double` row-source generic production branch | adopted-by-user | Phase 069 接入真实 row-source public overload，把 double branch 扩到 common PCL xyz AoS whitelist；Phase 074 已根据用户确认收口为 adopted。 | Phase 069 33-test correctness；QEMU row-source generic double probe Doctor `0/0/0`；board 9 个代表组合全 positive，median B/A `11.309x` 到 `17.433x`，Doctor `0/4/0`。 | 只覆盖 row-source common PCL xyz AoS whitelist / dense / 64K / 合法 index 或 correspondence；custom layout double、sorted-copy double 和任意自定义点型全集另开 phase。 |
| `Scalar=double` custom layout public scout | adopted-by-user | Phase 070 放宽 double dispatch 到 `RVVXYZAoSFloatLayout` sample，侦察一个测试本地 custom layout 的 ordered 与 row-source public path；Phase 074 已根据用户确认收口为 adopted。 | current 35-test correctness；QEMU custom layout double scout Doctor `0/0/0`；board 4 个 case 全 positive，median B/A `9.966x` 到 `27.497x`，Doctor `0/1/0`。 | 只覆盖 `LocalSVDScalePaddedXYZSource -> LocalSVDScaleWideXYZTarget`；不外推到全部 custom layout double、packed unaligned float、异常 alignment 全集、sorted-copy double 或任意自定义点型全集。 |
| `Scalar=double` more custom layout sampling | adopted-by-user | Phase 071 补 compact、huge-padding 和 aligned 三组测试本地 custom layout 的 ordered 与 row-source public sampling；Phase 074 已根据用户确认收口为 adopted。 | current 37-test correctness；QEMU more custom layout double sampling Doctor `0/0/0`；board 12 个 case 全 positive，median B/A `2.766x` 到 `29.087x`，Doctor `0/8/0`。 | 只作为 Phase 070 取样增强；不外推到全部 custom layout double、packed unaligned float、异常 alignment 全集、sorted-copy double 或任意自定义点型全集。 |
| `Scalar=double` correspondence sorted-copy | rolled_back_no_production | Phase 072 接入真实 public correspondence sorted-copy double probe并取得 public positive；Phase 073 同边界 detail A/B 已判定不支持 clean adoption；Phase 075 已回滚生产分流。 | current 38-test correctness；Phase 072 QEMU correspondence sorted-copy double Doctor `0/0/0`；board public probe 64K / 256K positive，median B/A `5.727x` / `5.408x`，Doctor `0/0/0`；Phase 073 detail A/B board median `0.283x` / `0.431x`，Doctor `2/1/0`；Phase 075 rollback 后 Std/RVV 38/38 passed。 | 当前 production 使用 D64 gather，不再尝试 sorted-copy double；Phase 072 public-positive 仅作为历史证据保留。Phase 047 `Scalar=float` sorted-copy 不受影响。 |

RVV 主路径在每个 VL chunk（可变向量长度分块）中读取 source / target 的 x、y、z，累加：

```text
source_sum[3]
target_sum[3]
source_target_sum[9] = Σ source * target^T
source_square_sum = Σ ||source||^2
```

然后标量后段计算：

```text
mean_s = source_sum / n
mean_t = target_sum / n
H = source_target_sum - n * mean_s * mean_t^T
R = V * U^T
sum_ss = source_square_sum - n * ||mean_s||^2
scale = trace(R * H) / sum_ss
translation = mean_t - scale * R * mean_s
```

`trace(R * H)` 与旧后段 `Σ target_demean · (R * source_demean)` 等价；Phase 020 作为实现形态诊断单独验证了这条等价式。

## Fallback 矩阵

| 条件 | 当前行为 | 证据 / 说明 |
| --- | --- | --- |
| 非 RVV 构建 | 父类 scale 标量路径。 | `__RVV10__` 外不编译 RVV helper。 |
| ordered exact `PointXYZ -> PointXYZ` 且 `Scalar=double` | 命中 Phase 066 double RVV branch；若 gate 失败则父类 scale 标量路径。 | `ScalarDoubleProductionProbeMatchesReference`、`ScalarDoubleProductionProbeFallbackBoundaries`；board scalar-double production probe median B/A `33.792x`。 |
| source-indexed / dual-indexed / correspondence exact `PointXYZ -> PointXYZ` 且 `Scalar=double` | 命中 Phase 067 row-source double RVV branch；若 gate 失败则父类 scale 标量路径。 | `ScalarDoubleRowSourceProductionProbeMatchesReference`、`ScalarDoubleRowSourceProductionProbeFallbackBoundaries`；board row-source scalar-double production probe median B/A `20.201x` / `14.800x` / `13.474x`。 |
| ordered common PCL xyz AoS whitelist 且 `Scalar=double` | 命中 Phase 068 ordered generic double RVV branch；若 gate 失败则父类 scale 标量路径。 | `GenericScalarDoubleOrderedProductionProbeMatchesReference`、`GenericScalarDoubleOrderedProductionProbeFallbackBoundaries`；board generic ordered double probe 5 case 全 positive。 |
| `Scalar=double` 但不在 adopted double gate 内 | 父类 scale 标量路径，或在 row-source legal gather gate 内走 D64 gather。 | Adopted scope 覆盖 exact `PointXYZ -> PointXYZ` ordered / row-source、ordered common PCL xyz AoS whitelist、row-source common PCL xyz AoS whitelist 与四组 custom layout samples。Phase 075 已回滚 correspondence sorted-copy double，当前合法 shuffled correspondence double 使用 D64 gather；其它 custom layout double、sorted-copy double family selection 或任意自定义点型全集仍未覆盖。 |
| 其它 `Scalar` | 父类 scale 标量路径。 | 当前 RVV production branches 只覆盖 `float` 和上述窄 `double`。 |
| 点型不满足 xyz AoS float layout | 父类 scale 标量路径。 | source / target 分别用 `RVVXYZAoSFloatLayout` gate。 |
| source / target 点数不等 | 父类 ordered overload 保持原错误语义。 | public fallback boundary test。 |
| `nr_points < 16` | 父类 scale 标量路径。 | `SmallInputFallsBack`。 |
| 任一 cloud 非 dense | 父类 scale 标量路径。 | `PublicScaleFallbackBoundariesMatchReference`。 |
| source variance 退化或 scale 非 finite | 父类 scale 标量路径。 | `DegenerateSourceFallsBack`。 |
| source-indexed / dual-indexed / correspondence | Phase 043 patch 尝试 RVV；gate 失败时回父类路径。 | `SourceIndexedScaleMatchesReference`、`DualIndexedScaleMatchesReference`、`CorrespondenceScaleMatchesReference`；用户已确认采纳。 |
| source-indexed contiguous source indices | source indices 为 step=1 contiguous slice 时使用 contiguous offset RVV accumulation；target 端保持 selected target 0 起点顺序。 | `AffineIndexFastPathPublicProbeMatchesReference`；Phase 061 QEMU / board production probe；非 contiguous 时回普通 source-indexed gather RVV path。 |
| dual-indexed contiguous source / target indices | 两组 indices 都是 step=1 contiguous 且 count 相同时使用 contiguous offset RVV accumulation。 | `AffineIndexFastPathPublicProbeMatchesReference`；Phase 061 QEMU / board production probe；任一侧不 contiguous 时回普通 dual-indexed gather RVV path。 |
| correspondence contiguous query / match | query 和 match 都是 step=1 contiguous slice 时使用 contiguous offset RVV accumulation。 | `AffineIndexFastPathPublicProbeMatchesReference`；Phase 061 QEMU / board production probe；不 contiguous 时继续普通 correspondence gather RVV path 或 sorted-copy gate。 |
| correspondence sorted-copy gate 不满足 | 继续使用普通 correspondence gather RVV path，若普通 RVV gate 失败再回父类路径。 | `CorrespondenceSortedCopyProductionProbeMatchesReference` 覆盖 64K shuffled probe；Phase 047 board 证明真实 public probe positive。 |
| staged-selected-cloud diagnostic | 不接入 production。 | Phase 048 board 6/6 negative，只保留 topic-local 诊断结论。 |
| dual-indexed source-sorted-copy diagnostic | 不接入 production。 | Phase 058 board 10-run median B/A `1.010x` 且 5/10 run 低于 1，只保留 topic-local 负向诊断结论。 |

## 数值算例

若中心化后的 source 只有两个点 `s0=(1,0,0)`、`s1=(-1,0,0)`，target 是 scale `2` 且 `R=I` 后的 `t0=(2,0,0)`、`t1=(-2,0,0)`：

```text
source_square_sum = 1^2 + (-1)^2 = 2
H(0,0) = 1*2 + (-1)*(-2) = 4
trace(R * H) = 4
scale = 4 / 2 = 2
```

RVV chunk 内每个 lane 贡献对应的 `sx*tx` 和 `sx*sx`，最终 ordered reduction（有序规约）得到同一数学量。真实测试使用多点三维非退化样本，覆盖旋转、平移和非 1 scale。

## Traceability Map

| 符号 / 文件 | 层级 | 作用 | 上游入口 | 下游消费者 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- | --- | --- |
| `TransformationEstimationSVDScale::estimateRigidTransformation` ordered overload | production public entry | RVV 尝试和父类 fallback。 | ordered cloud-pair caller。 | production RVV helper / 父类 ordered overload。 | production dispatch。 | `registration/include/pcl/registration/transformation_estimation_svd_scale.h`、`registration/include/pcl/registration/impl/transformation_estimation_svd_scale.hpp` |
| `TransformationEstimationSVDScale::estimateRigidTransformation` row-source overloads | adopted production public entry | source-indexed、dual-indexed 和 correspondence RVV 尝试，失败时父类 fallback。 | row-source public caller。 | row-source RVV helper / 父类对应 overload。 | production row-source dispatch。 | `registration/include/pcl/registration/transformation_estimation_svd_scale.h`、`registration/include/pcl/registration/impl/transformation_estimation_svd_scale.hpp` |
| `estimateRigidTransformationSVDScaleOrderedCloudPairRVV` | production helper | 检查 compile-time / runtime gate。 | scale ordered overload。 | RVV accumulation / scalar solve。 | fallback boundary。 | `registration/include/pcl/registration/impl/transformation_estimation_svd_scale.hpp` |
| `transformationEstimationSVDScaleNeedsCorrespondenceSortedCopy` | production helper | 判断 correspondence 是否足够大且 query order 呈 shuffle-like disorder。 | correspondence RVV helper。 | sorted-copy branch 或普通 gather branch。 | dispatch boundary。 | `registration/include/pcl/registration/impl/transformation_estimation_svd_scale.hpp` |
| `estimateRigidTransformationSVDScaleCorrespondencePairSortedCopyRVV` | production helper | 复制并排序 correspondence，再复用 correspondence RVV accumulation。 | correspondence RVV helper。 | `accumulateTransformationEstimationSVDScaleCorrespondencePairRVV` / scale solve helper。 | adopted production narrow branch。 | `registration/include/pcl/registration/impl/transformation_estimation_svd_scale.hpp` |
| `transformationEstimationSVDScaleContiguousIndexRange` / `transformationEstimationSVDScaleContiguousCorrespondenceRange` | production helper | 判断 indices 或 correspondence 是否为 step=1 contiguous slice，并返回 source / target offset。 | row-source RVV helpers。 | contiguous fast path 或普通 gather / sorted-copy branch。 | Phase 061 adopted dispatch boundary。 | `registration/include/pcl/registration/impl/transformation_estimation_svd_scale.hpp` |
| `accumulateTransformationEstimationSVDScaleContiguousOffsetPairRVV` | production RVV helper | 从 source / target offset 开始连续读取点对并累加 scale 统计量。 | source-indexed / dual-indexed / correspondence RVV helpers。 | scale solve helper。 | Phase 061 adopted RVV instruction path。 | `registration/include/pcl/registration/impl/transformation_estimation_svd_scale.hpp` |
| `accumulateTransformationEstimationSVDScaleOrderedCloudPairRVV` | production RVV helper | VL chunk 内累加 sum、cross sum 和 source square。 | production helper。 | scale solve helper。 | RVV instruction path。 | `registration/include/pcl/registration/impl/transformation_estimation_svd_scale.hpp` |
| `accumulateTransformationEstimationSVDScaleOrderedCloudPairD64RVV` | production RVV helper | 按 `SrcLayout` / `TgtLayout` offset 读取 ordered common PCL xyz AoS 的 float xyz，并扩宽到 f64 accumulation。 | ordered public double helper。 | `solveTransformationEstimationSVDScaleD64`。 | Phase 068 adopted ordered generic scalar-double RVV instruction path。 | `registration/include/pcl/registration/impl/transformation_estimation_svd_scale.hpp` |
| `accumulateTransformationEstimationSVDScaleSourceIndexedPointXYZD64RVV` / `accumulateTransformationEstimationSVDScaleDualIndicesPointXYZD64RVV` / `accumulateTransformationEstimationSVDScaleCorrespondencePointXYZD64RVV` | production RVV helpers | row-source exact `PointXYZ -> PointXYZ` / `Scalar=double` 的 f64 widened accumulation；contiguous slice 先走 offset fast path，非 contiguous 合法输入走 widened gather。 | row-source public double overloads。 | double scale solve helper。 | Phase 067 adopted RVV instruction path。 | `registration/include/pcl/registration/impl/transformation_estimation_svd_scale.hpp` |
| `solveTransformationEstimationSVDScaleF32` | production scalar tail | 用累加量构造 `H`、SVD、scale 和 transform。 | RVV accumulation。 | public output matrix。 | numeric equivalence / fallback gate。 | `registration/include/pcl/registration/impl/transformation_estimation_svd_scale.hpp` |
| `src/test_tesvd_scale.cpp` | topic-local correctness | public path、fallback 和 candidate 对拍。 | `run_test_compare`。 | QEMU logs / registry。 | correctness gate。 | `test-rvv/registration/transformation_estimation_svd_scale/src/test_tesvd_scale.cpp` |
| `src/bench_tesvd_scale.cpp` | topic-local bench | public-scale、diagnostic、matrix-local、generic public、more-generic public、row-source、row-source all-more-generic、ordered / row-source / generic / custom layout scalar-double 和 sorted-copy case-filter。 | QEMU smoke / board repeated target。 | summary / manifest scripts。 | log shape / board performance。 | `test-rvv/registration/transformation_estimation_svd_scale/src/bench_tesvd_scale.cpp` |
| `generate_tesvd_scale_board_repeated_summary.py` | topic-local script | 生成 board summary、manifest 和 doctor 输入。 | board repeated target。 | Evidence Doctor / registry。 | evidence summary。 | `test-rvv/registration/transformation_estimation_svd_scale/script/generate_tesvd_scale_board_repeated_summary.py` |
| Phase 031 result | topic-local closeout | 记录用户确认采纳后的同步结果。 | adoption closeout。 | README / roadmap / long-term doc。 | recovery pointer。 | `test-rvv/registration/transformation_estimation_svd_scale/doc/phases/031-adoption-closeout-plan/result.zh.md` |
| Phase 062 result | topic-local closeout | 记录 Phase 061 contiguous affine index fast path 经用户确认后的 adoption closeout。 | adoption closeout。 | README / roadmap / long-term doc。 | recovery pointer。 | `test-rvv/registration/transformation_estimation_svd_scale/doc/phases/062-affine-index-fast-path-adoption-closeout/result.zh.md` |

## 正确性与高效性证据链

| evidence layer | 证据 | 结论 |
| --- | --- | --- |
| correctness | `make -C test-rvv/registration/transformation_estimation_svd_scale run_test_compare`：Std/RVV 38 tests passed。 | 真实 public ordered path、fallback boundary、row-source public path、correspondence sorted-copy production probe、affine contiguous diagnostic / production branch、ordered / row-source / ordered generic / row-source generic / custom layout scalar-double production probe、Phase 071 more custom layout sampling、Phase 072 correspondence sorted-copy scalar-double bounded probe、row-source representative generic path、row-source all-more-generic full matrix、custom layout padding / alignment sensitivity、sorted-copy / target-sorted / Phase 058 stability correctness guard、diagnostic candidate、matrix-local equivalence 和代表泛型点型均保持正确。 |
| QEMU smoke | Phase 010 public-scale smoke Doctor `Errors=0`、`Warnings=0`；当前裸 `log/qemu/evidence_doctor.md` 可被其它 smoke target 覆盖。 | QEMU 只证明 build、路径和日志形状，不作为性能结论。 |
| ASM attribution | production ordered overload 符号内可见 `vlsseg3e32.v`、`vfmacc.vv`、`vfredosum.vs`、`vsetvli`。 | 真实 production path 中存在 RVV load / FMA / reduction 指令。 |
| board production direct | `test-rvv/registration/transformation_estimation_svd_scale/log/board/production_public_scale_ordered_cloud_pair_repeated/summary.md`：4K/64K/256K median B/A = `26.123x` / `33.860x` / `33.066x`。 | 在 Milkv-Jupiter 板卡上，已采纳范围内 public RVV path 明显快于 public scalar path。 |
| board generic public | `test-rvv/registration/transformation_estimation_svd_scale/log/board/generic_xyz_point_types_public_repeated/summary.md`：8 个代表点型 case median B/A 均为 positive。 | 在 Milkv-Jupiter 板卡上，代表泛型点型 public RVV path 明显快于 public scalar path；不外推到全部点型。 |
| QEMU more-generic public | `test-rvv/registration/transformation_estimation_svd_scale/log/qemu/more_generic_xyz_aos_point_types_public/evidence_doctor.md`：5 个 common PCL xyz AoS smoke comparisons，Doctor `Errors=0`、`Warnings=0`、`Suggestions=0`。 | Phase 051 扩大这些具体点型组合的 correctness / QEMU smoke 证据边界；QEMU timing 不作为性能结论。 |
| board more-generic public | `test-rvv/registration/transformation_estimation_svd_scale/log/board/more_generic_xyz_aos_point_types_public_repeated/summary.md`：5 个 common PCL xyz AoS case 全部 positive，median B/A 范围 `20.555x` 到 `26.989x`；Doctor `Errors=0`、`Warnings=0`、`Suggestions=0`。 | Phase 052 证明这 5 个具体 ordered public 点型组合在板卡上仍有正向性能；不外推到全部自定义点型、row-source 点型扩展或 `Scalar=double`。 |
| board row-source public | `test-rvv/registration/transformation_estimation_svd_scale/log/board/row_source_scale_repeated/summary.md`：9 个 row-source case median B/A 均为 positive，范围 `6.171x` 到 `16.309x`。 | 在 Milkv-Jupiter 板卡上，当前 `PointXYZ -> PointXYZ` row-source public RVV path 明显快于 public scalar path；用户已确认采纳。 |
| board row-source generic public | `test-rvv/registration/transformation_estimation_svd_scale/log/board/row_source_generic_xyz_point_types_repeated/summary.md`：9 个代表点型 row-source case median B/A 均为 positive，范围 `6.325x` 到 `10.371x`。 | 在 Milkv-Jupiter 板卡上，代表点型 row-source public RVV path 明显快于 public scalar path；不外推到全部 xyz AoS。 |
| board row-source all-more-generic public | `test-rvv/registration/transformation_estimation_svd_scale/log/board/row_source_all_more_generic_xyz_aos_matrix_repeated/summary.md`：45 个常见点型 × row-source case 全部 positive，overall median B/A 范围 `7.277x` 到 `12.565x`；Doctor `Errors=0`、`Warnings=18`、`Suggestions=0`。 | 在 Milkv-Jupiter 板卡上，Phase 051 五个常见 PCL xyz AoS 点型组合在三类 row source 下仍有正向性能；不外推到全部自定义点型、异常 layout / padding 或 `Scalar=double`。 |
| board custom layout alignment sampling | `test-rvv/registration/transformation_estimation_svd_scale/log/board/custom_layout_alignment_sensitivity_repeated/summary.md`：6 个 alignas custom layout × row-source case 全部 positive，median B/A 范围 `1.636x` 到 `2.309x`；Doctor `Errors=0`、`Warnings=3`、`Suggestions=0`。 | 在 Milkv-Jupiter 板卡上，一个 alignment-sensitive custom layout 采样仍为 positive；该证据只扩大采样边界，不扩大 production gate。 |
| board row-source locality profile | `test-rvv/registration/transformation_estimation_svd_scale/log/board/row_source_locality_order_profile_repeated/summary.md`：36 个 row source / order / size case 全 positive；shuffle 明显低于 contiguous / stride / reverse。 | 当前 row-source public RVV path 对 locality / order pattern 敏感；这解释 Evidence Doctor warning，但不否定已采纳范围。 |
| board correspondence sorted-copy probe | `test-rvv/registration/transformation_estimation_svd_scale/log/board/correspondence_sorted_copy_production_probe_repeated/summary.md`：4K/64K/256K median B/A = `8.128x` / `3.929x` / `3.618x`。 | 在 correspondence + shuffle-like disorder + size gate 的生产公开入口上，sorted-copy probe 已形成 adopted positive 子边界。 |
| QEMU affine index fast path production probe | `test-rvv/registration/transformation_estimation_svd_scale/log/qemu/row_source_affine_index_fast_path_production_probe/evidence_doctor.md`：6 comparisons，Doctor `Errors=0`、`Warnings=0`、`Suggestions=0`。 | QEMU 只证明 public probe label、路径和日志形状，不作为性能结论。 |
| board affine index fast path production probe | `test-rvv/registration/transformation_estimation_svd_scale/log/board/row_source_affine_index_fast_path_production_probe_repeated/summary.md`：6 个 contiguous row-source case 全部 positive，median B/A `10.115x` 到 `14.021x`；Doctor `Errors=0`、`Warnings=0`、`Suggestions=0`。 | 在 step=1 contiguous source-indexed、dual-indexed 和 correspondence 公开入口上，已采纳 fast path 明显快于 public scalar baseline；不外推到 stride / reverse / shuffle。 |
| QEMU scalar-double production probe | `test-rvv/registration/transformation_estimation_svd_scale/log/qemu/scalar_double_production_probe/evidence_doctor.md`：Doctor `Errors=0`、`Warnings=0`、`Suggestions=0`。 | QEMU 只证明 ordered double public probe label、路径、误差和 ASM 输入形状，不作为性能结论。 |
| board scalar-double production probe | `test-rvv/registration/transformation_estimation_svd_scale/log/board/scalar_double_production_probe_repeated/summary.md`：5-run B/A `33.955, 33.664, 33.781, 34.077, 33.792`，median `33.792x`；Doctor `Errors=0`、`Warnings=0`、`Suggestions=0`。 | 在 Milkv-Jupiter 板卡上，ordered exact `PointXYZ -> PointXYZ` / `Scalar=double` / dense public RVV branch 明显快于 public double scalar fallback；不外推到 generic double。 |
| QEMU row-source scalar-double production probe | `test-rvv/registration/transformation_estimation_svd_scale/log/qemu/row_source_scalar_double_production_probe/evidence_doctor.md`：Doctor `Errors=0`、`Warnings=0`、`Suggestions=0`。 | QEMU 只证明 row-source double public probe label、路径、误差和 ASM 输入形状，不作为性能结论。 |
| board row-source scalar-double production probe | `test-rvv/registration/transformation_estimation_svd_scale/log/board/row_source_scalar_double_production_probe_repeated/summary.md`：source-indexed / dual-indexed / correspondence median B/A `20.201x` / `14.800x` / `13.474x`；Doctor `Errors=0`、`Warnings=1`、`Suggestions=0`。 | 在 Milkv-Jupiter 板卡上，三类 row-source exact `PointXYZ -> PointXYZ` / `Scalar=double` / dense public RVV branch 明显快于 public double scalar fallback；dual-indexed variance warning 不改变 positive bucket；不外推到 generic double、custom layout double 或 sorted-copy double。 |
| QEMU generic scalar-double ordered production probe | `test-rvv/registration/transformation_estimation_svd_scale/log/qemu/generic_scalar_double_ordered_production_probe/evidence_doctor.md`：Doctor `Errors=0`、`Warnings=0`、`Suggestions=0`。 | QEMU 只证明 ordered generic double public probe label、路径、误差和 ASM 输入形状，不作为性能结论。 |
| board generic scalar-double ordered production probe | `test-rvv/registration/transformation_estimation_svd_scale/log/board/generic_scalar_double_ordered_production_probe_repeated/summary.md`：5 个 representative common PCL xyz AoS case 全 positive，median B/A `24.111x` 到 `32.493x`；Doctor `Errors=0`、`Warnings=1`、`Suggestions=0`。 | 在 Milkv-Jupiter 板卡上，ordered common PCL xyz AoS whitelist / `Scalar=double` / dense public RVV branch 明显快于 public double scalar fallback；`PointNormal->PointXYZRGB` group-outlier warning 只要求按点型组合解释，不外推到 row-source generic double 或 custom layout double。 |
| QEMU row-source generic scalar-double production probe | `test-rvv/registration/transformation_estimation_svd_scale/log/qemu/row_source_generic_scalar_double_production_probe/evidence_doctor.md`：9 comparisons，Doctor `Errors=0`、`Warnings=0`、`Suggestions=0`。 | QEMU 只证明 row-source generic double public probe label、路径和误差输入形状，不作为性能结论。 |
| board row-source generic scalar-double production probe | `test-rvv/registration/transformation_estimation_svd_scale/log/board/row_source_generic_scalar_double_production_probe_repeated/summary.md`：9 个 representative common PCL xyz AoS row-source case 全 positive，median B/A `11.309x` 到 `17.433x`；Doctor `Errors=0`、`Warnings=4`、`Suggestions=0`。 | 在 Milkv-Jupiter 板卡上，row-source common PCL xyz AoS whitelist / `Scalar=double` / dense public RVV branch 明显快于 public double scalar fallback；warning 要求保留 min / median / max，不外推到 custom layout double 或 sorted-copy double。 |
| QEMU custom layout scalar-double scout | `test-rvv/registration/transformation_estimation_svd_scale/log/qemu/custom_layout_scalar_double_diagnostic_scout/evidence_doctor.md`：4 comparisons，Doctor `Errors=0`、`Warnings=0`、`Suggestions=0`。 | QEMU 只证明一个 custom layout double sample 的 public label、路径和误差输入形状，不作为性能结论。 |
| board custom layout scalar-double scout | `test-rvv/registration/transformation_estimation_svd_scale/log/board/custom_layout_scalar_double_diagnostic_scout_repeated/summary.md`：4 个 public overload case 全 positive，median B/A `9.966x` 到 `27.497x`；Doctor `Errors=0`、`Warnings=1`、`Suggestions=0`。 | 该证据只覆盖 `LocalSVDScalePaddedXYZSource -> LocalSVDScaleWideXYZTarget`；Phase 074 已根据用户确认收口为 adopted，不外推到全部 custom layout double。 |
| QEMU more custom layout scalar-double sampling | `test-rvv/registration/transformation_estimation_svd_scale/log/qemu/more_custom_layout_scalar_double_sampling/evidence_doctor.md`：12 comparisons，Doctor `Errors=0`、`Warnings=0`、`Suggestions=0`。 | QEMU 只证明更多 custom layout double sampling 的 public label、路径和误差输入形状，不作为性能结论。 |
| board more custom layout scalar-double sampling | `test-rvv/registration/transformation_estimation_svd_scale/log/board/more_custom_layout_scalar_double_sampling_repeated/summary.md`：12 个 compact / huge-padding / aligned custom layout case 全 positive，median B/A `2.766x` 到 `29.087x`；Doctor `Errors=0`、`Warnings=8`、`Suggestions=0`。 | 该证据只作为 Phase 070 取样增强；long-tail 和 group-outlier warning 要求按 layout / row source 分开解释，不能外推到全部 custom layout double。 |
| board staged-selected-cloud detail A/B | `test-rvv/registration/transformation_estimation_svd_scale/log/board/row_source_shuffle_staged_selected_cloud_detail_ab_repeated/summary.md`：6 个 shuffled row-source case 全 negative。 | staged-selected-cloud 不是生产行为；它只证明该 mitigation family 当前不值得继续接入。 |
| board dual-indexed target-sorted detail A/B | `test-rvv/registration/transformation_estimation_svd_scale/log/board/row_source_shuffle_dual_indexed_target_sorted_detail_ab_repeated/summary.md`：64K median B/A `0.897x`，256K median B/A `1.170x`。 | target-sorted 不是生产行为；64K negative 且 256K 仅 weak-positive，不支撑 dual-indexed production probe。 |
| board dual-indexed 256K source-sorted-copy stability | `test-rvv/registration/transformation_estimation_svd_scale/log/board/row_source_shuffle_dual_indexed_256k_sorted_copy_stability_repeated/summary.md`：10-run median B/A `1.010x`，5/10 run 低于 1。 | source-sorted-copy 不是 production behavior；它关闭 Phase 046 残留 dual-indexed 256K weak-positive 线索，不支撑 dual-indexed production probe。 |
| Evidence Doctor | `test-rvv/registration/transformation_estimation_svd_scale/log/board/production_public_scale_ordered_cloud_pair_repeated/evidence_doctor.md`：`Errors=0`、`Warnings=1`、`Suggestions=0`。 | 4K group_outlier 需要按 size 分开解释；4K 自身最小 B/A 仍为 `25.902x`，不阻塞当前窄范围采纳。 |
| registry | `make -C test-rvv/registration/transformation_estimation_svd_scale evidence_status` 为 fresh。 | 文档引用的 summary / doctor / correctness 证据已登记。 |

## Bench case 说明

`public-scale` case-filter 计时真实 public ordered scale path，包含公开入口 dispatch、父类标量 baseline、RVV fused accumulation、3x3 SVD、scale 和输出矩阵。B/A = Std public scale ms / RVV public scale ms；大于 1 表示 RVV production path 更快。

`ordered-cloud-pair` case-filter 是 Phase 000 test-only diagnostic，证明直接 fused accumulation 有生产价值线索，但不替代 production direct evidence。

`matrix-local-scale` case-filter 是 Phase 020 / 042 implementation-shape diagnostic，只比较 legacy formula 和 `trace(R * H)` 局部公式形态；Phase 050 已采纳 production helper simplification，但该 case-filter 仍不证明 RVV intrinsic 收益。

`generic-xyz-point-types-public` case-filter 是 Phase 041 production public generic evidence，覆盖 `PointXYZI` / `PointXYZRGB` 代表点型同型和混合组合。

`more-generic-xyz-aos-point-types-public` case-filter 是 Phase 051 / 052 more-generic public evidence，覆盖更多常见 PCL xyz AoS 点型组合。Phase 051 用它生成 correctness / smoke / manifest，Phase 052 用同一 case-filter 生成 board repeated summary。

`row-source-scale` case-filter 是 Phase 043 production public row-source evidence，覆盖 source-indexed、dual-indexed 和 correspondence 三类公开入口的 `PointXYZ -> PointXYZ` / `float` / dense 输入。

`row-source-generic-xyz-point-types` case-filter 是 Phase 044 production public row-source representative generic evidence，覆盖 source-indexed `PointXYZI -> PointXYZI`、dual-indexed `PointXYZRGB -> PointXYZRGB` 和 correspondence `PointXYZI -> PointXYZRGB`。

`row-source-all-more-generic-xyz-aos-matrix` case-filter 是 Phase 054 production public row-source all-more-generic evidence，覆盖 Phase 051 五个常见 PCL xyz AoS 点型组合在 source-indexed、dual-indexed 和 correspondence 下的全交叉输入。它扩大 evidence boundary（证据边界），不新增 production behavior。

`row-source-locality-order-profile` case-filter 是 Phase 045 production public row-source profile evidence，覆盖 source-indexed、dual-indexed 和 correspondence 在 contiguous、stride、reverse 与 deterministic shuffle 下的 `PointXYZ -> PointXYZ` / `float` / dense 输入。它解释 locality sensitivity，不是新的 RVV family。

`row-source-affine-index-fast-path-production-probe` case-filter 是 Phase 061 production public affine fast path evidence，覆盖 source-indexed、dual-indexed 和 correspondence 在 step=1 contiguous indices / correspondences 下的 64K / 256K 输入。它通过真实 public overload 计时，证明 contiguous offset fast path 的有界收益；Phase 062 根据用户确认把它收口为 adopted production branch。

`scalar-double-production-probe` case-filter 是 Phase 065 / 066 production public scalar-double evidence，覆盖 ordered exact `PointXYZ -> PointXYZ` / `Scalar=double` / dense / 64K 输入。它通过真实 public ordered overload 计时，证明 f64 widened accumulation 的有界收益；Phase 066 根据用户确认把它收口为 adopted production branch。

`scalar-double-row-source-production-probe` case-filter 是 Phase 067 production public row-source scalar-double evidence，覆盖 source-indexed、dual-indexed 和 correspondence 的 exact `PointXYZ -> PointXYZ` / `Scalar=double` / dense / 64K 输入。它通过真实 public row-source overload 计时，证明 row-source f64 widened accumulation 的有界收益；Phase 067 根据用户确认把它收口为 adopted production branch。

`generic-scalar-double-ordered-production-probe` case-filter 是 Phase 068 production public ordered generic scalar-double evidence，覆盖 `PointXYZI->PointXYZI`、`PointXYZRGB->PointXYZRGB`、`PointXYZI->PointXYZRGB`、`PointXYZRGBA->PointXYZRGBA` 和 `PointNormal->PointXYZRGB` 的 `Scalar=double` / dense / 64K 输入。它通过真实 public ordered overload 计时，证明 common PCL xyz AoS whitelist 下 f64 widened accumulation 的有界收益；Phase 068 根据用户确认把它收口为 adopted production branch。

`row-source-generic-scalar-double-production-probe` case-filter 是 Phase 069 production public row-source generic scalar-double evidence，覆盖 source-indexed、dual-indexed 和 correspondence 下的 common PCL xyz AoS whitelist / `Scalar=double` / dense / 64K 输入。它通过真实 public row-source overload 计时，证明 row-source generic f64 widened accumulation 的有界收益；Phase 074 已根据用户确认收口为 `adopted-by-user`。

`custom-layout-scalar-double-diagnostic-scout` case-filter 是 Phase 070 production public custom layout scalar-double scout，覆盖 `LocalSVDScalePaddedXYZSource -> LocalSVDScaleWideXYZTarget` 的 ordered 与三类 row-source public overload。它通过真实 public overload 计时，但只证明这一组测试本地 registered layout sample；Phase 074 已根据用户确认收口为 adopted。

`more-custom-layout-scalar-double-sampling` case-filter 是 Phase 071 production public more custom layout scalar-double sampling，覆盖 compact、huge-padding 和 aligned 三组测试本地 registered layout sample 的 ordered 与三类 row-source public overload。它是 Phase 070 的取样增强，不证明全部 custom layout double。

`correspondence-sorted-copy-production-probe` case-filter 是 Phase 047 production public row-source probe evidence，覆盖 correspondence 在 deterministic shuffle 下的 4K / 64K / 256K 输入。它通过真实 public correspondence overload 计时，证明 sorted-copy branch 的有界收益。

`row-source-shuffle-staged-selected-cloud-detail-ab` case-filter 是 Phase 048 production detail RVV-vs-RVV diagnostic，比较 current shuffled row-source RVV 与 selected-cloud staging 后的 ordered public RVV。该路线已 negative，不是已采纳生产行为。

`row-source-shuffle-dual-indexed-target-sorted-detail-ab` case-filter 是 Phase 049 production detail RVV-vs-RVV diagnostic，比较 current shuffled dual-indexed RVV 与 target-sorted copy 后的同一 public dual-indexed RVV。该路线 64K negative、256K weak-positive，整体不支撑 production。

`row-source-shuffle-dual-indexed-256k-sorted-copy-stability` case-filter 是 Phase 058 production detail RVV-vs-RVV diagnostic，只复核 dual-indexed 256K source-sorted-copy 残留线索。10-run board 显示收益贴近 1 且退化频率高，因此不进入 production probe。

## 当前不覆盖的方向

后续方向必须独立建 phase，并补同边界 correctness、QEMU、ASM、board 和 Evidence Doctor：

- `generic-point-type-expansion`：代表点型 `PointXYZI` / `PointXYZRGB` 已有 public board evidence；Phase 051 / 052 已补更多常见 PCL xyz AoS 点型 correctness / QEMU smoke / board repeated，Phase 054 已补这些点型组合的 row-source 全交叉 evidence，Phase 055 / 056 / 057 / 059 已补有限 custom layout / padding / alignment 采样。若要覆盖全部自定义 xyz AoS、row-source 更广点型或异常 layout / alignment 取样，仍需独立 phase。
- `row-source-expansion`：Phase 043 已有 `PointXYZ -> PointXYZ` adopted evidence；Phase 044 已补代表泛型点型；Phase 045 已证明 locality / order profile 对 B/A 仍有显著影响；Phase 047 只接入 correspondence sorted-copy 窄分支；Phase 061/062 只接入 step=1 contiguous affine index fast path；Phase 048 已拒绝 staged-selected-cloud；Phase 049 已拒绝 dual-indexed target-sorted；Phase 058 已拒绝 dual-indexed 256K source-sorted-copy。后续若继续，必须另开新的 candidate family 或更窄诊断，不能把 sorted-copy 或 contiguous affine fast path 外推到全部 row-source / index 分布。
- `matrix-local-scale-simplification`：已作为 helper 简化接入；后续只维护数值等价和 fallback 边界，不作为新的 RVV family 扩展。
- `Scalar=double`：ordered exact `PointXYZ -> PointXYZ`、三类 row-source exact `PointXYZ -> PointXYZ`、ordered common PCL xyz AoS whitelist、row-source common PCL xyz AoS whitelist 和四组 custom layout double sampling 已采纳。correspondence sorted-copy double 已由 Phase 075 回滚，当前 production 使用 D64 gather。更广输入规模、更多 custom layout double、任意自定义点型全集和新的 sorted-copy double family selection 仍需要独立数值预算 / f64 性能计划。

## 采用边界

当前 patch 尚未自动提交。提交前应保持 topic、production source、summary evidence 和 raw logs 的边界分离：默认不提交 build output、raw board logs、本机 `config.mk` 或 unrelated dirty worktree。若后续扩大 row source、点类型、`Scalar` 或实现族选择，必须重新进入 phase loop，不能继承本 ordered production evidence。
