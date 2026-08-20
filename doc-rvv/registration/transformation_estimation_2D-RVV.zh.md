# transformation_estimation_2D RVV 生产接入记录

## 当前生产状态

`registration/include/pcl/registration/impl/transformation_estimation_2D.hpp`
当前 production patch（生产补丁）接入四条已采纳 / 保留的 RVV 路径：

| row source | 当前生产状态 | 范围 |
| --- | --- | --- |
| ordered-cloud-pair | adopted / retained | traits-gated PointXYZ-like AoS `x/y/z`，`Scalar=float`。 |
| source-indexed-cloud-pair | adopted / retained | exact `PointXYZ -> PointXYZ`，`Scalar=float`，valid source indices。 |
| source-indexed-cloud-pair | adopted / retained | exact `PointXYZI -> PointXYZI`，`Scalar=float`，valid source indices；Phase 110 证据为正向，Phase 112 按用户确认采纳。 |
| dual-indexed-cloud-pair | adopted / retained | exact `PointXYZ -> PointXYZ`，`Scalar=float`，valid source / target indices；Phase 109 保留 4K caveat。 |
| correspondence-pair | not adopted | Phase 107 试接入后 family A/B 仍有 256K 退化频率，production dispatch 已退回到原标量 iterator 路径。 |

这里的 adopted / retained 只说明当前已确认或本轮被授权接入并准备提交的生产补丁范围，不表示所有
PCL 点型、所有 row source（行来源）、`Scalar=double` 或自定义 traits 点型都已获得生产证据。

## Gate 与 fallback

ordered-cloud-pair public overload（source/target 按相同下标一一对应）在以下条件同时满足时尝试 RVV：

- `Scalar == float`；
- `PointSource` 和 `PointTarget` 分别满足 `pcl::rvv::RVVXYZAoSFloatLayout<PointT>`；
- source / target 点数相等且不少于 16；
- 两侧 `is_dense == true`；
- 两侧所有点的 `x/y/z` 都是 finite（有限值）。

source-indexed public overload 只在以下窄范围尝试 RVV：

- adopted gate：`PointSource == pcl::PointXYZ`、`PointTarget == pcl::PointXYZ`；
- adopted Phase 112 gate：`PointSource == pcl::PointXYZI`、`PointTarget == pcl::PointXYZI`；
- `Scalar == float`；
- `sizeof(pcl::index_t) == sizeof(std::int32_t)`；
- `indices_src.size() == cloud_tgt.size()`，点数不少于 16；
- 所有 source index 有效；
- source / target dense；
- selected source rows 和 target prefix rows 全部 finite。

dual-indexed public overload 只在以下窄范围尝试 RVV：

- `PointSource == pcl::PointXYZ`、`PointTarget == pcl::PointXYZ`；
- `Scalar == float`；
- `sizeof(pcl::index_t) == sizeof(std::int32_t)`；
- `indices_src.size() == indices_tgt.size()`，点数不少于 16；
- 所有 source / target index 有效；
- source / target dense；
- selected source / target rows 全部 finite。

任一 gate 失败时，公开入口继续使用原有 `ConstCloudIterator` 标量路径。correspondence
public overload 当前没有 RVV production dispatch。

## 实现形态

三条 RVV helper 都保持公开 API 不变。热点部分分两遍处理：

1. 用 source / target 各自的 layout offset 和 stride / indexed byte offset 读取 `x/y/z`，
   并规约 `x/y` 质心。
2. 再次读取 `x/y`，对中心化后的 `H00/H01/H10/H11` 做向量 FMA（融合乘加）累加。

`atan2`、`cos/sin`、平移计算和 4x4 矩阵写回保持标量。RVV helper 返回 `false` 时，
公开入口自然回退到原标量 iterator helper。

## 正确性与高效性证据链

接入后 correctness（正确性）：

```bash
make -C test-rvv/registration/transformation_estimation_2D run_test_compare
```

结果：Std `84/84` pass，RVV `84/84` pass。correspondence family 测试按当前生产事实验证
不同实现族在误差预算内一致，不再要求 scalar iterator、ordered RVV 和 dual-indexed RVV 逐位 checksum 相同。

QEMU smoke / asm / Evidence Doctor（证据体检）：

| 路径 | 命令 | Doctor | 反汇编归属 |
| --- | --- | --- | --- |
| ordered public | `record_qemu_production_public_state` | `0/0/0` | `production_public_generic_boundary`，generic symbols 可见。 |
| source-indexed public | `record_qemu_source_indexed_public_state` | `0/0/0` | `production_public_source_indexed_boundary`，46 RVV lines。 |
| source-indexed PointXYZI public | `record_qemu_source_indexed_pointxyzi_public_state` | `0/0/0` | `production_public_source_indexed_generic_boundary` 命中 `PointXYZI -> PointXYZI` source-indexed public overload；focused category 85 RVV lines。 |
| dual-indexed family A/B | `record_qemu_dual_indexed_family_ab_state` | `0/0/0` | `production_public_dual_indexed_boundary`，50 RVV lines。 |

QEMU 只用于路径命中、日志形状和反汇编归属，不作为性能结论。

Milkv-Jupiter board repeated：

| 路径 | run label | 结果摘要 | Doctor |
| --- | --- | --- | --- |
| ordered generic public | `generic_xyz_point_types_public_phase107_repeated` | 16 cases 全部 positive，`B/A<1=0/5`；代表值：`PointXYZ->PointXYZ` 4K/64K/256K 为 `4.400x / 5.556x / 5.184x`。 | `0/4/0`，均为长尾或组内 outlier Warning。 |
| source-indexed exact public | `source_indexed_public_phase107_repeated` | 4K/64K/256K 为 `4.157x / 4.814x / 4.615x`，`B/A<1=0/5`。 | `0/0/0` |
| source-indexed PointXYZI exact public | `source_indexed_pointxyzi_public_phase110_repeated` | 4K/64K/256K 为 `3.979x / 3.547x / 3.602x`，`B/A<1=0/20`。 | `0/3/0`，均为长尾或方差 Warning；Phase 112 已采纳 exact gate。 |
| dual-indexed exact family A/B | `dual_indexed_family_ab_phase109_variance_repeated` | direct dual-indexed 相对 materialize+ordered 的 B/A：4K `1.080x`、64K `1.661x`、256K `1.646x`；4K 有 `1/20` below-1，64K/256K `0/20` below-1。 | `0/3/0`，Warning 均在 4K。 |
| correspondence exact family A/B | `correspondence_family_ab_phase107_repeated` | 4K `1.091x`、64K `1.645x`、256K `1.385x`，但 256K 有 `4/20` below-1，decision bucket 为 negative。 | `0/4/0`；用于解释退回，不作为 production adoption。 |

Phase 109 已为 dual-indexed exact 4K caveat 完成独立 20-run variance。该证据支持保留
current exact dual-indexed dispatch，但 4K 仍有长尾和 `1/20` below-1，因此长期文档继续保留
小规模 caveat，不把结论扩大到泛型点型或 correspondence。

对应摘要路径位于：

```text
test-rvv/registration/transformation_estimation_2D/log/board/generic_xyz_point_types_public_phase107_repeated/summary.md
test-rvv/registration/transformation_estimation_2D/log/board/source_indexed_public_phase107_repeated/summary.md
test-rvv/registration/transformation_estimation_2D/log/board/source_indexed_pointxyzi_public_phase110_repeated/summary.md
test-rvv/registration/transformation_estimation_2D/log/board/dual_indexed_family_ab_phase109_variance_repeated/summary.md
test-rvv/registration/transformation_estimation_2D/log/board/correspondence_family_ab_phase107_repeated/summary.md
```

## 未采纳或已退回的方向

source-indexed exact `PointXYZI -> PointXYZI` 已采纳：Phase 110 使用独立
`source_indexed_pointxyzi_public_phase110_repeated` 20-run board evidence 后为 positive，
Phase 112 按用户确认把该 exact gate 写成 adopted / retained。该采纳只覆盖 exact
`PointXYZI -> PointXYZI`，不能把它写成 full source-indexed generic adoption。

source-indexed generic PointXYZ-like widening 不采纳：Phase 103/104 仍只是 guarded probe
（受保护探针）；Phase 106 使用独立 `source_indexed_generic_xyz_point_types_public_variance_repeated`
20-run public representative variance evidence 后，出现 12 个 positive、1 个 weak_positive、
3 个 negative，board Doctor `1/27/0`。其中 `PointNormal -> PointNormal 256K`
median `1.574x`，但 `B/A<1=7/20`。Phase 111 进一步审计确认 `PointNormal` 和
`PointXYZINormal` 都是 48B stride，负向样本集中在这些 Normal 类组合；它们不能继承
Phase 110 `PointXYZI` 32B exact probe 的正向证据。因此 full source-indexed generic widening
不能 clean-adopt，Normal 类 source-indexed dispatch 也不接入生产。

当前可解释的主要原因是访存形态，而不是 correctness（正确性）或 2D 公式错误。source-indexed
helper 每个向量 chunk 先读 source indices，把 index 转成 byte offset，再对 source 做 indexed
gather（索引离散加载），同时对 target prefix 做 strided load（跨步加载）；centroid 和
correlation 又各扫描一遍。当前 `indexed_load3_f32m2` / `strided_load3_f32m2` 为通用 xyz
路径同时加载 `x/y/z`，但 2D 估计实际只使用 `x/y`。在 `PointNormal` /
`PointXYZINormal` 这类 48B AoS stride（数组结构步长）点型上，source gather、target stride
和多余 `z` load 叠加后更容易受缓存局部性、内存带宽、TLB 和 gather latency（离散加载延迟）
影响；因此 median 仍可能为正，但 below-1 频率和 long-tail 方差足以阻止生产泛型门控。

可能的后续优化方式包括 2D 专用 load2（只加载 `x/y`）、exact Normal 特化、把 selected
`x/y` 先 materialize（物化）到紧凑 buffer，或对 source-indexed Normal 做分块 / profile
消融。这些方向都没有当前生产采纳计划：用户已明确不继续推进 Normal 类 source-indexed 优化，
本 topic 只把它们记录为未推进的候选，不作为默认下一 phase。

dual-indexed generic PointXYZ-like widening 不采纳：Phase 100 production-shaped diagnostic
为 negative，不能把 Phase 107 exact dual-indexed patch 扩成泛型点类型 production dispatch。

correspondence direct RVV 不采纳：Phase 107 correspondence public 曾正向，但同一
production-detail family A/B 在 256K 仍有退化频率，已按用户规则退回 production dispatch。
Phase 095-098 的 staged-dual、locality/order、component ablation 和 chunked xyz staging
也没有形成可接入的稳定替代。

`Scalar=double`、RGB/RGBA、自定义 traits 点型、其它内建 xyz-like 点型和非法 index /
correspondence 安全合同扩展均未覆盖。

## 维护边界

后续若继续扩大范围，需要重新建立独立 phase 和独立证据目录，至少覆盖 correctness、
fallback、QEMU smoke、asm、board repeated、Evidence Doctor、registry 和本文件同步。
负向或 guarded 证据不能写成 adopted production behavior。
