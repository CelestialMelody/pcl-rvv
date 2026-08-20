# transformation_estimation_2D RVV 生产实现说明

## 当前生产状态

`TransformationEstimation2D` 当前保留四类 RVV production behavior（生产行为）：

| 公开入口 / row source | 当前状态 | 生产范围 | 证据边界 |
| --- | --- | --- | --- |
| `estimateRigidTransformation(cloud_src, cloud_tgt, matrix)` / ordered-cloud-pair（顺序点云对） | adopted / retained | `Scalar=float`，source / target 分别满足 `RVVXYZAoSFloatLayout<PointT>`，dense finite，点数不少于 16。 | Phase 107 ordered generic public board 16/16 positive；当前 correctness Std/RVV `84/84`。 |
| `estimateRigidTransformation(cloud_src, indices_src, cloud_tgt, matrix)` / source-indexed-cloud-pair（源索引点云对） | adopted / retained | exact `PointXYZ -> PointXYZ`，`Scalar=float`，valid source indices，dense finite，点数不少于 16。 | Phase 091/107 public board 4K/64K/256K `4.157x / 4.814x / 4.615x`，Doctor `0/0/0`。 |
| 同上 | adopted / retained | exact `PointXYZI -> PointXYZI`，`Scalar=float`，valid source indices，dense finite，点数不少于 16。 | Phase 110/112 public board 20-run 4K/64K/256K `3.979x / 3.547x / 3.602x`，`B/A<1=0/20`，Doctor `0/3/0`。 |
| `estimateRigidTransformation(cloud_src, indices_src, cloud_tgt, indices_tgt, matrix)` / dual-indexed-cloud-pair（双索引点云对） | adopted / retained with caveat | exact `PointXYZ -> PointXYZ`，`Scalar=float`，valid source / target indices，dense finite，点数不少于 16。 | Phase 109 family A/B 20-run `1.080x / 1.661x / 1.646x`；4K 有 `1/20` below-1，保留小规模 caveat。 |
| `estimateRigidTransformation(cloud_src, cloud_tgt, correspondences, matrix)` / correspondence-pair（对应关系点对） | scalar-only | 不保留 RVV production dispatch。 | Phase 107 trial 后同边界 family A/B 256K `4/20` below-1，已退回标量 iterator 路径。 |

这些 adopted / retained 状态只覆盖表中列出的入口、点型、`Scalar`、layout（布局）和 row source。它们不表示 full generic source-indexed widening（完整泛型扩大）、Normal 类点型、`Scalar=double`、RGB/RGBA、自定义 traits 点型或 correspondence dispatch 已获得生产证据。

## 函数语义和标量路径

公开入口先把不同输入形态规整成两条逐行遍历的 source / target stream：

- ordered-cloud-pair：第 `k` 行使用 `cloud_src[k]` 和 `cloud_tgt[k]`。
- source-indexed-cloud-pair：第 `k` 行使用 `cloud_src[indices_src[k]]` 和 `cloud_tgt[k]`。
- dual-indexed-cloud-pair：第 `k` 行使用 `cloud_src[indices_src[k]]` 和 `cloud_tgt[indices_tgt[k]]`。
- correspondence-pair：第 `k` 行使用 `cloud_src[index_query]` 和 `cloud_tgt[index_match]`。

没有命中 RVV gate 时，公开入口创建 `ConstCloudIterator`，进入 protected helper：

1. `compute3DCentroid` 分别计算 source 和 target 的 3D centroid（质心）。
2. 2D 估计忽略 z 平移，把 source / target centroid 的 z 分量置零。
3. `demeanPointCloud` 分别生成 source / target 的 demean matrix（去中心化矩阵）。
4. `cloud_src_demean * cloud_tgt_demean.transpose()` 形成 correlation matrix（相关矩阵）`H`。
5. 用 `atan2(H01 - H10, H00 + H11)` 求 2D rotation angle（旋转角）。
6. 用 `cos/sin` 和 `centroid_tgt - R * centroid_src` 写回 4x4 transform matrix（变换矩阵）。

热点成本主要在逐点 centroid、demean matrix 写出和 correlation matrix 乘法。`atan2`、`cos`、`sin` 和 4x4 matrix 写回每次估计只执行一次，当前保持标量实现。

## Gate 与 fallback

RVV production helper 只在 `__RVV10__` 构建下启用。任一 gate 失败时，公开入口继续调用原 `ConstCloudIterator` 标量路径，公开 API 和非 RVV 构建行为不变。

| gate | ordered-cloud-pair | source-indexed | dual-indexed |
| --- | --- | --- | --- |
| `Scalar` | 只允许 `float` | 只允许 `float` | 只允许 `float` |
| 点型 / layout | source / target 分别满足 `RVVXYZAoSFloatLayout<PointT>` | 只允许 exact `PointXYZ -> PointXYZ` 或 exact `PointXYZI -> PointXYZI`，同时满足 layout gate | 只允许 exact `PointXYZ -> PointXYZ` |
| index 类型 | not applicable | `sizeof(pcl::index_t) == sizeof(std::int32_t)` | 同 source-indexed |
| 规模 | source / target size 相等且不少于 16 | `indices_src.size() == cloud_tgt.size()` 且不少于 16 | `indices_src.size() == indices_tgt.size()` 且不少于 16 |
| dense / finite | source / target dense，所有点 finite | source / target dense，selected source rows 和 target prefix finite | source / target dense，selected source / target rows finite |
| index 安全 | not applicable | 所有 source index 在 cloud 范围内 | 所有 source / target index 在 cloud 范围内 |
| fallback | 标量 protected helper | 标量 source-indexed iterator helper | 标量 dual-indexed iterator helper |

correspondence overload 当前不尝试 RVV helper。Phase 107 的 correspondence trial 已从 production dispatch 中退回。

## 当前采用的优化方式

当前 RVV helper 都采用 two-pass centered 2D correlation accumulator（两遍中心化 2D 相关项累加器）。它不再显式写出两份 4xN demean matrix，也不调用 Eigen 矩阵乘法来生成 `H`，而是在 RVV loop 中直接求 2D centroid 和 2x2 correlation。

| 阶段 | RVV 做什么 | 保留标量的部分 | 采用理由 |
| --- | --- | --- | --- |
| gate 前检查 | 模板 gate 和 runtime gate 决定是否进入 RVV。 | 错误消息、公开入口尺寸检查和 fallback 仍沿用原路径。 | 不改变 public semantics（公开语义）。 |
| pass 1: centroid | 每个 VL chunk load source / target 的 `x/y/z`，只累加 `x/y`，用 `vfredosum` 做向量规约。 | finite 检查在进入 RVV loop 前由标量循环完成。 | 与 `compute3DCentroid` 的有限值语义保持一致，同时避免生成临时矩阵。 |
| pass 2: correlation | 再次 load `x/y/z`，对 `sx-csx`、`sy-csy`、`tx-ctx`、`ty-cty` 做四个 `vfmacc` 累加：`H00/H01/H10/H11`。 | `atan2`、`cos/sin`、translation 和 matrix write 保持标量。 | 热点在逐点累加；solver tail 每次只执行一次，向量化收益小且数值边界更复杂。 |
| row source load | ordered 使用 strided segment load；source-indexed 使用 source indexed gather + target strided load；dual-indexed 使用 source / target 双侧 indexed gather。 | correspondence 不进入 RVV production dispatch。 | row source 逐条独立批准，防止 ordered 证据外推到 indexed / correspondence。 |

当前 `strided_load3_f32m2` / `indexed_load3_f32m2` 是通用 xyz load3（加载 x/y/z 三个字段）。2D 公式实际只使用 `x/y`，但 z 仍用于进入 RVV 前的 finite 语义检查和当前通用加载 helper。这个实现形态在 ordered 和两个 exact indexed gate 上已有证据；在 source-indexed Normal 类 full generic widening 上没有通过方差证据。

## VL chunk 示例

假设一次调用只有 4 对 2D 点，且目标硬件某次 `vsetvl_e32m2` 返回 `vl=4`：

```text
source: (1,1), (2,1), (3,1), (4,1)
target: (2,1), (4,1), (6,1), (8,1)
```

pass 1 在一个 chunk 中累加：

```text
source_sum_x = 10, source_sum_y = 4
target_sum_x = 20, target_sum_y = 4
source_centroid = (2.5, 1.0)
target_centroid = (5.0, 1.0)
```

pass 2 对同一 chunk 做中心化乘加：

```text
centered_source_x = [-1.5, -0.5, 0.5, 1.5]
centered_source_y = [0, 0, 0, 0]
centered_target_x = [-3, -1, 1, 3]
centered_target_y = [0, 0, 0, 0]
H00 = sum(centered_source_x * centered_target_x) = 10
H01 = H10 = H11 = 0
```

后段标量解算得到 `atan2(0, 10)`，旋转角为 0；translation 为 `target_centroid - R * source_centroid = (2.5, 0)`。实际点数大于 `vl` 时，loop 会按 `vl` 分块累加，每个 chunk 的 partial vector sum 最后用 `vfredosum` 归约到标量。

source-indexed 和 dual-indexed 的数学流程相同，差异只在每个 chunk 如何从 index stream 生成 byte offset 并加载 source / target rows。

## 范围决策表

| 范围 | 当前决策 | 证据 / 原因 | 恢复条件 |
| --- | --- | --- | --- |
| ordered PointXYZ-like generic，`Scalar=float` | adopted / retained | Phase 107 public board 16/16 positive；current correctness `84/84`。 | 扩到未逐类型上板或特殊语义点型时另建 phase。 |
| source-indexed exact `PointXYZ -> PointXYZ` | adopted / retained | Phase 091 用户采纳；Phase 107 public board `4.157x / 4.814x / 4.615x`。 | 扩到其它点型时另建 phase。 |
| source-indexed exact `PointXYZI -> PointXYZI` | adopted / retained | Phase 110/112 20-run positive，`B/A<1=0/20`。 | 只覆盖 exact gate；不能继承为泛型 widening。 |
| source-indexed generic PointXYZ-like | guarded / not adopted | Phase 103/104 是 guarded probe；Phase 106 representative 20-run 为 12 positive、1 weak_positive、3 negative，Doctor `1/27/0`。 | 需要新的同边界 representative evidence，并消除 negative bucket / Doctor Error。 |
| source-indexed Normal 类 | rejected for current gate / not planned | Phase 111 审计确认 `PointNormal` / `PointXYZINormal` 48B stride 负向风险；用户已决定不继续推进 Normal 优化。 | 只有用户明确重开，才另建 exact Normal probe 或 profiling phase。 |
| dual-indexed exact `PointXYZ -> PointXYZ` | adopted / retained with 4K caveat | Phase 109 20-run positive；4K `1/20` below-1。 | 泛型点型和 4K caveat 解除都需要独立证据。 |
| dual-indexed generic PointXYZ-like | rejected / not adopted | Phase 100 diagnostic negative，不能从 exact gate 外推。 | 只有新的 bounded production probe 和完整 PI1-PI5 证据。 |
| correspondence exact RVV | rolled back / scalar-only | Phase 107 family A/B 256K `4/20` below-1；Phase 095-098 多条替代路线 negative。 | 只有新的同边界 candidate、独立 phase 和完整证据链。 |
| `Scalar=double` | scalar-only | 当前 RVV helper 只验证 `float`。 | double-specific correctness、asm、board 和数值预算。 |
| RGB/RGBA、自定义 traits 点型 | unvalidated / fallback | 当前证据不覆盖颜色语义、写回语义或未逐类型上板 layout。 | 点型 inventory、fallback tests、asm、board repeated 和 Doctor。 |

## Source-indexed generic widening 为什么不接入

Phase 106 使用真实 public source-indexed generic probe 做 20-run representative variance（代表性方差证据）。结论不是 correctness failure，而是 performance stability failure（性能稳定性失败）：16 个代表 case 中有 3 个 negative bucket，`PointNormal->PointNormal 256K` median 仍为 `1.574x`，但 `B/A<1=7/20`，Board Doctor 为 `1/27/0`。

受证据约束的主要原因是访存形态：

- source-indexed 每个 chunk 先加载 source indices，再把 index 转成 byte offset，对 source 做 indexed gather（索引离散加载）。
- target 侧按 prefix row 做 strided load（跨步加载）。
- centroid pass 和 correlation pass 都要扫描一次。
- 通用 load3 同时加载 `x/y/z`，但 2D 求解只使用 `x/y`。
- `PointNormal` / `PointXYZINormal` 是 48B AoS stride，比 `PointXYZI` 的 32B stride 和 `PointXYZ` 的 16B stride 更重。

这些因素叠加后，Normal 类在 source gather、target stride、cache locality（缓存局部性）、memory bandwidth（内存带宽）、TLB 和 gather latency（离散加载延迟）上更容易出现 long tail（长尾）。因此 exact `PointXYZI -> PointXYZI` 的正向证据不能证明 Normal 类或 full generic source-indexed gate 可采纳。

可想象的技术方向包括 2D-only load2（只加载 `x/y`）、exact Normal specialization（Normal 具体点型特化）、先把 selected `x/y` materialize（物化）到紧凑 buffer，或对 Normal source-indexed 做 chunk/profile ablation（分块 / 剖析消融）。这些方向当前记录为 not planned：没有生产证据，且用户已决定不再推进 Normal 类 source-indexed 优化探索。

## Bench 与证据

性能结论只引用 board / target hardware（目标硬件） repeated evidence。QEMU 只用于 correctness、log shape（日志形状）和 asm attribution（反汇编归属）。

| 证据 | 命令 / target | 当前结果 | 角色 |
| --- | --- | --- | --- |
| correctness | `make -C test-rvv/registration/transformation_estimation_2D record_qemu_correctness_state` | Std `84/84` pass，RVV `84/84` pass。 | public semantics、fallback 和当前 production gate correctness。 |
| QEMU / asm ordered public | `record_qemu_production_public_state` | Doctor `0/0/0`；`production_public_generic_boundary` 可归属。 | 路径命中和指令归属，不证明性能。 |
| QEMU / asm source-indexed PointXYZI | `record_qemu_source_indexed_pointxyzi_public_state` | Doctor `0/0/0`；focused `production_public_source_indexed_generic_boundary` 85 RVV lines。 | exact `PointXYZI -> PointXYZI` 路径命中。 |
| ordered generic board | `record_board_generic_xyz_point_types_public_state` | 16 cases positive；代表 `PointXYZ->PointXYZ` 为 `4.400x / 5.556x / 5.184x`；Doctor `0/4/0`。 | ordered generic production performance。 |
| source-indexed exact `PointXYZ` board | `record_board_source_indexed_public_state` | `4.157x / 4.814x / 4.615x`；Doctor `0/0/0`。 | source-indexed exact production performance。 |
| source-indexed exact `PointXYZI` board | `record_board_source_indexed_pointxyzi_public_state TE2D_BOARD_REPEATED_RUNS=20` | `3.979x / 3.547x / 3.602x`；`B/A<1=0/20`；Doctor `0/3/0`。 | exact PointXYZI adoption evidence。 |
| source-indexed generic public variance | `record_board_source_indexed_generic_xyz_point_types_public_variance_state TE2D_BOARD_REPEATED_RUNS=20` | 12 positive、1 weak_positive、3 negative；Doctor `1/27/0`。 | full generic widening negative evidence。 |
| dual-indexed exact family A/B | `record_board_dual_indexed_family_ab_state` with Phase 109 label | `1.080x / 1.661x / 1.646x`；4K `1/20` below-1；Doctor `0/3/0`。 | direct dual-indexed family retention evidence。 |
| correspondence family A/B | Phase 107 correspondence family target | `1.091x / 1.645x / 1.385x`，256K `4/20` below-1；Doctor `0/4/0`。 | trial rollback evidence。 |

Generated summary / Doctor / asm files live under `test-rvv/registration/transformation_estimation_2D/log/**` and are regenerated by the Make targets above. They are not part of the default topic commit; committed documents record run label、target、decision bucket 和关键数值，raw/generated logs 留在本机或由 future evidence-log phase 单独脱敏审计。

## 正确性与高效性证据链

| 层级 | 结论 | 边界 |
| --- | --- | --- |
| correctness | 当前 Std / RVV aggregate 均为 `84/84` pass。 | 证明当前 public semantics corpus、fallback 和 adopted gate 没有回归；不证明性能。 |
| QEMU / asm | adopted helper 和 guarded / rejected probe 均有路径归属记录；current PointXYZI exact QEMU Doctor `0/0/0`。 | QEMU timing 不作为性能结论。 |
| board performance | adopted ordered、source-indexed exact `PointXYZ`、source-indexed exact `PointXYZI` 和 dual-indexed exact 都有 repeated board evidence。 | dual-indexed 4K caveat 保留；source-indexed generic / Normal / correspondence 负向不能被正向 exact gate 覆盖。 |
| Evidence Doctor | adopted exact paths没有 Error；部分 Warning 是长尾或小规模方差，已写入 caveat。 | Phase 106 source-indexed generic 有 Doctor Error，阻止 clean adoption。 |
| fallback / risk | 非 RVV 构建、`Scalar` 非 float、未覆盖点型、非 dense、非 finite、小规模或非法 index 都回退标量路径。 | invalid correspondence safety 没有被扩大成新 public contract。 |

## Traceability Map

| 符号 / 文件 | 层级 | 作用 | 调用者 / 上游入口 | 被调用者 / 下游消费者 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- | --- | --- |
| ordered public overload | production public entry | ordered-cloud-pair dispatch 和 fallback。 | PCL registration 调用方。 | ordered RVV helper 或 protected scalar helper。 | adopted ordered production boundary。 | `registration/include/pcl/registration/impl/transformation_estimation_2D.hpp` |
| source-indexed public overload | production public entry | exact `PointXYZ` / `PointXYZI` source-indexed dispatch 和 fallback。 | PCL registration 调用方。 | source-indexed RVV helper 或 source-indexed scalar iterator。 | adopted exact source-indexed production boundary。 | 同上 |
| dual-indexed public overload | production public entry | exact `PointXYZ` dual-indexed dispatch 和 fallback。 | PCL registration 调用方。 | dual-indexed RVV helper 或 dual-indexed scalar iterator。 | retained exact dual-indexed production boundary。 | 同上 |
| correspondence public overload | production public entry | 当前直接走 scalar iterator。 | PCL registration 调用方。 | protected scalar helper。 | correspondence scalar-only boundary。 | 同上 |
| `tryTransformationEstimation2DOrderedCloudPairRVV` | production RVV helper | traits-gated ordered two-pass accumulator。 | ordered public overload。 | RVV load helpers、reduction helper、scalar solver tail。 | adopted implementation detail。 | 同上 |
| `tryTransformationEstimation2DSourceIndexedCloudPairRVV` | production RVV helper | exact source-indexed gather + target stride two-pass accumulator。 | source-indexed public overload。 | RVV indexed / strided load helpers。 | adopted exact `PointXYZ` / `PointXYZI` implementation detail。 | 同上 |
| `tryTransformationEstimation2DDualIndexedCloudPairRVV` | production RVV helper | source / target 双侧 indexed gather two-pass accumulator。 | dual-indexed public overload。 | RVV indexed load helpers。 | retained exact dual-indexed implementation detail。 | 同上 |
| `reduceTransformationEstimation2DF32M2` | production RVV helper | `vfredosum` vector reduction 到标量。 | 三个 RVV helper。 | scalar centroid / correlation fields。 | reduction attribution。 | 同上 |
| `test-rvv/registration/transformation_estimation_2D/src/test_te2d.cpp` | correctness tests | public semantics、fallback、family equivalence 和 generic negative-case correctness。 | Make correctness targets。 | registry / phase docs。 | correctness gate。 | `test-rvv/registration/transformation_estimation_2D/src/test_te2d.cpp` |
| `test-rvv/registration/transformation_estimation_2D/src/bench_te2d.cpp` | bench wrapper | ordered、source-indexed、dual-indexed、correspondence、generic 和 family A/B bench labels。 | Make QEMU / board targets。 | summary scripts / Evidence Doctor。 | benchmark input and timer boundary。 | `test-rvv/registration/transformation_estimation_2D/src/bench_te2d.cpp` |
| summary generator scripts | analysis script | 生成 board repeated summary / manifest。 | board collect targets。 | Evidence Doctor / registry。 | generated evidence boundary。 | `test-rvv/registration/transformation_estimation_2D/script/` |
| Evidence registry | evidence index | 检查 registered evidence freshness。 | `make evidence_status`。 | Handoff / phase result。 | freshness gate；local generated path。 | `test-rvv/registration/transformation_estimation_2D/log/evidence_registry.json` |
| Phase 106 result | phase result | source-indexed generic public representative variance negative。 | Phase 106 plan。 | evaluation、matrix、当前文档。 | rejected generic widening evidence。 | `test-rvv/registration/transformation_estimation_2D/doc/phases/106-source-indexed-generic-public-variance/result.zh.md` |
| Phase 111 result | phase result | Normal 类负向原因审计。 | Phase 111 plan。 | roadmap、matrix、当前文档。 | Normal not-adopted evidence。 | `test-rvv/registration/transformation_estimation_2D/doc/phases/111-source-indexed-normal-negative-case-investigation/result.zh.md` |
| Phase 112 result | phase result | exact `PointXYZI -> PointXYZI` adoption closeout。 | Phase 112 plan。 | evaluation、matrix、当前文档。 | adopted PointXYZI closeout evidence。 | `test-rvv/registration/transformation_estimation_2D/doc/phases/112-source-indexed-pointxyzi-adoption-closeout/result.zh.md` |
| Phase 113 result | phase result | production doc closeout 和 generated log tracking cleanup。 | Phase 113 plan。 | Handoff、提交边界。 | document closeout / log hygiene evidence。 | `test-rvv/registration/transformation_estimation_2D/doc/phases/113-doc-rvv-closeout-and-log-hygiene/result.zh.md` |
| evaluation | topic-local decision doc | 函数级评估、候选取舍、诊断证据链和生产判断。 | phase docs。 | 当前长期文档 / Handoff。 | decision audit。 | `test-rvv/registration/transformation_estimation_2D/doc/transformation_estimation_2D-evaluation.zh.md` |
| optimization matrix | phase matrix | candidate × row source × point type × evidence 状态。 | phase results。 | roadmap / Handoff。 | scope and decision index。 | `test-rvv/registration/transformation_estimation_2D/doc/phases/optimization-matrix.zh.md` |

## 生产 closeout

| 项 | 最终状态 | 证据 |
| --- | --- | --- |
| production 文件 | 只修改 `registration/include/pcl/registration/impl/transformation_estimation_2D.hpp`；不改变 public API。 | current production source。 |
| helper | 三个 RVV helper：ordered、source-indexed exact、dual-indexed exact；一个 vector reduction helper。 | QEMU asm 和 correctness target。 |
| dispatch | `__RVV10__` 下先尝试 RVV，成功即返回；失败继续原标量 helper。 | fallback tests 和 current Std/RVV `84/84`。 |
| adopted scope | ordered generic、source-indexed exact `PointXYZ`、source-indexed exact `PointXYZI`、dual-indexed exact `PointXYZ`。 | Phase 107 / 109 / 110 / 112 evidence。 |
| rolled back / scalar-only | correspondence production RVV。 | Phase 107 family A/B negative；Phase 095-098 alternatives negative。 |
| not adopted | source-indexed generic / Normal、dual-indexed generic、correspondence generic、`Scalar=double`、custom / RGB / RGBA。 | Phase 100 / 101 / 106 / 111 and fallback boundaries。 |
| log policy | generated `log/**` 不进入默认 topic commit；需要时另开 evidence-log phase。 | Phase 113 log hygiene。 |

## 后续方向

当前 topic 没有默认继续的 Normal / correspondence 优化动作。后续只有在用户明确重开新 phase 时才恢复：

- source-indexed Normal exact probe 或 profiling：需要独立 board label / evidence dir、correctness、QEMU、asm、20-run board 和 Evidence Doctor。
- 2D-only load2、selected `x/y` materialization 或 chunk/profile ablation：只作为可能的技术路线记录，不是当前计划。
- correspondence 新 bounded candidate：必须先证明同边界 candidate 比当前 scalar / existing RVV family 稳定，再进入 PI1-PI5。
- `Scalar=double` 或更多点型 inventory：必须逐点型 / layout / row source 单独批准。
