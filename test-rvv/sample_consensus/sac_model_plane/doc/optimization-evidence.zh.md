# sac_model_plane 优化证据索引

## 当前结论摘要

Phase 000 的 indexed xyz gather（按索引离散加载）+ shared distance kernel（共享距离内核）
已采纳为 production 行为。Phase 010 又把 identity-index strided load（恒等索引跨步加载）
窄采纳到 `selectWithinDistanceRVV` 和 `countWithinDistanceRVV`。`getDistancesToModelRVV`
仍保持 gather-only，因为该入口的 identity 分支没有形成稳定收益。

## 优化方式总表

| candidate family | 代码路径 | 测试 / bench | board / asm evidence | decision | 边界 / 下一步 |
| --- | --- | --- | --- | --- | --- |
| indexed xyz gather + shared distance kernel | `sac_model_plane.hpp` 三个 RVV helper | `run_test_compare`、`board_smoke`、5-run repeated board | asm 中有 `vluxei32` / `vfmacc` / `vcompress`；Phase 000 board 三项 positive | adopted | 覆盖 direct indexed `PointXYZ` board，`PointXYZI` correctness。 |
| select `vcompress` writeback | `selectWithinDistanceRVV` | `PublicEntriesMatchDirectRVVForSupportedLayout` | select Phase 000 median 3.1965x，Phase 010 identity median 3.3896x | adopted | 输出顺序和 buffer shrink 已闭合。 |
| count mask + `vcpop` | `countWithinDistanceRVV` | public entry 对拍 + board bench | count Phase 000 median 1.6678x，Phase 010 identity median 2.1969x | adopted | identity strided load 对该入口收益最大。 |
| dense distance RVV store | `getDistancesToModelRVV` | public entry 对拍 + board bench | Phase 000 median 2.3695x，asm 有 `vfwcvt` / `vse64` / `vluxei32` | adopted | 保持 gather-only；不采纳 identity strided load。 |
| identity-index strided load for getDistances | withdrawn branch | mixed-gate board rerun + current asm check | identity narrowed report有 long-tail Warning；current asm 无 `vlsseg3e32` | rejected with evidence | 只有出现低开销 identity 检测或同边界 RVV-vs-RVV positive 时才重开。 |
| additional PointXYZ-like types | `src/test_sac_model_plane.cpp` | `AdditionalAoSPointTypesMatchStandardPath`、`AdditionalAoSPointTypesIdentityMatchStandardPath` | QEMU Std/RVV 各 7 个 gtest 通过；板卡 RVV gtest 7/7 | adopted for correctness | `PointXYZRGB/RGBA`、`PointXYZINormal` 代表性 correctness 已闭合；性能不外推。 |
| explicit empty `indices_` | `src/test_sac_model_plane.cpp` | `ExplicitEmptyIndicesMatchStandardPath` | QEMU Std/RVV 各 7 个 gtest 通过；板卡 RVV gtest 7/7 | adopted for correctness | 只证明显式空子集 public entry 输出清空和 Standard 对齐，不新增性能结论。 |

## 标量路径与 RVV 路径差异

Standard helper 每次按 `(*indices_)[i]` 构造 `Eigen::Vector4f` 并计算 dot。RVV helper 先加载
一段 `indices_`，把 index 转成 32-bit byte offset，再按当前点型的 `x/y/z` offset gather，
随后用 `distRVV_f32m2` 计算 `abs(ax+by+cz+d)`。`selectWithinDistanceRVV` 使用 mask +
`vcompress` 保序写回；`countWithinDistanceRVV` 使用 `vcpop` 计数；`getDistancesToModelRVV`
把 float 距离拓宽为 double 后写入输出。

Phase 010 的 `sacModelPlaneRVVLoadXYZ` 只在调用者传入 `indices_may_be_identity=true` 且当前 VL
chunk 全部满足 identity 时切到 `strided_load3_f32m2`。目前只有 select/count 传入 `true`；
getDistances 传入 `false`，因此没有 identity 分流成本。

## 结论边界

当前采纳不覆盖 normal-plane、sphere/circle、line/stick、SAC 后处理、correspondence row source、
`Scalar=double`、非 AoS layout 或代表点型之外的自定义点型全集。后续优化如果改写 load strategy
（加载策略）或新增点型性能结论，必须以新 phase 的证据为准。
