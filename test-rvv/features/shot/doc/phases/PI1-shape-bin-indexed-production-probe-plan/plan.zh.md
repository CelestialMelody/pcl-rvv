# PI1 plan: shape-bin indexed production probe

## 阶段意图和边界

本阶段只写 production integration plan（生产接入计划），不修改 `features/include/pcl/features/impl/shot.hpp`。目标是把 Phase 020/030/040 的 shape-bin component diagnostic（组件诊断）证据转成一个受控的 production probe（生产探针）范围，供用户决定是否授权 PI2 production patch（生产补丁）。

`validated_scope`：Phase 040 已验证 `pcl::PointCloud<pcl::Normal>` + `pcl::Indices` 的 indexed gather helper，固定 `frame_z`，double bin distance 输出，NaN normal count 和 `shape_bin_indexed_component` board A/B。

`unvalidated_scope`：真实 `createBinDistanceShape` 对 `normals_`、`indices`、`frames_` 和 `PCL_WARN` 的对象状态访问；`SHOTEstimation` / `SHOTColorEstimation` public entry；泛型 point type（点类型）和 `Scalar=double`；小规模 fallback；production asm hotspot attribution（生产热点反汇编归属）。

## 候选范围

| area | PI1 scope |
| --- | --- |
| production entry | 只考虑 `createBinDistanceShape` 内部 shape-bin distance 计算，不改公开 API。 |
| point type / layout | 初始 production probe 推荐收窄到 `pcl::Normal` AoS indexed gather；若要泛型接入，必须先使用 normal traits / POD / standard-layout / offset gate。 |
| Scalar | 当前输出和 production helper 使用 `double` bin distance；不把其它 `Scalar` 路径写成已覆盖。 |
| row source | 只覆盖 `pcl::Indices` 指向 normal cloud 的 indexed row source。 |
| side effect | 必须保留 NaN normal count，并在 count 非零时保留 production `PCL_WARN` 语义。 |
| forbidden expansion | 不扩大到 descriptor interpolation、color LAB distance、LRF、OMP 文件、public API 或跨 topic 公共 helper 变更。 |

## Fallback / dispatch 计划

| gate | proposed behavior | evidence needed before PI5 |
| --- | --- | --- |
| 非 RVV 构建 | 原标量路径保持不变。 | Std build correctness。 |
| point type gate 不满足 | fallback 到原标量 `createBinDistanceShape` 逻辑。 | fallback unit test 或 exact gate 下的非覆盖路径构建证据。 |
| `normals_` / `indices` 规模超出 32-bit byte offset | fallback 标量；若依赖 PCL index validity（索引有效性）前置条件，仍需证明 `normals_->size() <= UINT32_MAX / sizeof(PointT)`。 | boundary test。 |
| 小规模邻域 | 可 fallback 标量，阈值需由 board probe 决定。 | small-count correctness + bench。 |
| NaN / Inf normal | RVV finite mask 输出 NaN bin，并累加 warning count；warning 文本保持 production 现有语义。 | production direct side-effect test。 |
| public SHOT color path | 只在 shape channel 共用路径命中；color-specific 逻辑保持标量。 | SHOT1344 production direct correctness / bench。 |

## Production direct tests 计划

| test | purpose |
| --- | --- |
| fixed-LRF public SHOT352 RVV hit | 证明公开入口命中 shape-bin RVV 分流，descriptor 有限且 L2 归一。 |
| fixed-LRF public SHOT1344 RVV hit | 证明 color descriptor 共享 shape path 时仍保持 correctness。 |
| NaN normal warning count | 证明 RVV path 与标量路径保留同样的 NaN 输出和 warning side effect。 |
| fallback compile / runtime | 证明非 RVV、非覆盖点类型、小规模或 offset gate 失败时自然回退标量。 |

## 反汇编和板卡计划

| evidence | command / boundary |
| --- | --- |
| production asm | `dump_bench_rvv` 或等价 production bench asm，必须把 `vluxei32.v` / `vcpop.m` 归属到 production helper 或其内联 callsite。 |
| production board smoke | public fixed-LRF SHOT352 / SHOT1344 targeted compare，不能只沿用 component case。 |
| production board rerun | 若 public entry 或 production-detail A/B > 1.10x，最多追加 2 次 targeted rerun；若接近阈值，扩大 run budget 或降级。 |
| Evidence Doctor | 生产证据必须重新生成 manifest / doctor，Errors 必须为 0；Warnings 必须解释。 |

## PI2 前暂停条件

当前阶段结束后必须暂停，等待用户明确授权是否修改 `features/include/pcl/features/impl/shot.hpp`。授权前不得进入 PI2，也不得更新 `doc-rvv/features/shot-RVV.zh.md` 为 adopted production behavior（已采用生产行为）。

若用户授权 PI2，默认只允许实现上述 shape-bin indexed production probe；任何扩大到 interpolation、normalization、color path 专项、LRF 或 OMP 的动作都需要另行 phase plan。
