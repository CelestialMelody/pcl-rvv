# transformation_estimation_2D RVV 生产接入记录

## 当前生产状态

`registration/include/pcl/registration/impl/transformation_estimation_2D.hpp`
当前 production patch（生产补丁）接入三条 RVV 路径：

| row source | 当前生产状态 | 范围 |
| --- | --- | --- |
| ordered-cloud-pair | adopted / retained | traits-gated PointXYZ-like AoS `x/y/z`，`Scalar=float`。 |
| source-indexed-cloud-pair | adopted / retained | exact `PointXYZ -> PointXYZ`，`Scalar=float`，valid source indices。 |
| dual-indexed-cloud-pair | adopted for current topic-only commit | exact `PointXYZ -> PointXYZ`，`Scalar=float`，valid source / target indices。 |
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

- `PointSource == pcl::PointXYZ`、`PointTarget == pcl::PointXYZ`；
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
| dual-indexed family A/B | `record_qemu_dual_indexed_family_ab_state` | `0/0/0` | `production_public_dual_indexed_boundary`，50 RVV lines。 |

QEMU 只用于路径命中、日志形状和反汇编归属，不作为性能结论。

Milkv-Jupiter board repeated：

| 路径 | run label | 结果摘要 | Doctor |
| --- | --- | --- | --- |
| ordered generic public | `generic_xyz_point_types_public_phase107_repeated` | 16 cases 全部 positive，`B/A<1=0/5`；代表值：`PointXYZ->PointXYZ` 4K/64K/256K 为 `4.400x / 5.556x / 5.184x`。 | `0/4/0`，均为长尾或组内 outlier Warning。 |
| source-indexed exact public | `source_indexed_public_phase107_repeated` | 4K/64K/256K 为 `4.157x / 4.814x / 4.615x`，`B/A<1=0/5`。 | `0/0/0` |
| dual-indexed exact family A/B | `dual_indexed_family_ab_phase107_repeated` | direct dual-indexed 相对 materialize+ordered 的 B/A：4K `1.085x`、64K `1.691x`、256K `1.678x`；4K 有 `1/5` below-1。 | `0/3/0`，Warning 均在 4K。 |
| correspondence exact family A/B | `correspondence_family_ab_phase107_repeated` | 4K `1.091x`、64K `1.645x`、256K `1.385x`，但 256K 有 `4/20` below-1，decision bucket 为 negative。 | `0/4/0`；用于解释退回，不作为 production adoption。 |

对应摘要路径位于：

```text
test-rvv/registration/transformation_estimation_2D/log/board/generic_xyz_point_types_public_phase107_repeated/summary.md
test-rvv/registration/transformation_estimation_2D/log/board/source_indexed_public_phase107_repeated/summary.md
test-rvv/registration/transformation_estimation_2D/log/board/dual_indexed_family_ab_phase107_repeated/summary.md
test-rvv/registration/transformation_estimation_2D/log/board/correspondence_family_ab_phase107_repeated/summary.md
```

## 未采纳或已退回的方向

source-indexed generic PointXYZ-like widening 不采纳：Phase 103/104 仍只是 guarded probe
（受保护探针）；Phase 106 使用独立 `source_indexed_generic_xyz_point_types_public_variance_repeated`
20-run public representative variance evidence 后，出现 12 个 positive、1 个 weak_positive、
3 个 negative，board Doctor `1/27/0`。其中 `PointNormal -> PointNormal 256K`
median `1.574x`，但 `B/A<1=7/20`。因此 full source-indexed generic widening
不能 clean-adopt，本轮生产 helper 已收窄回 Phase 091 exact `PointXYZ -> PointXYZ` gate。

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
