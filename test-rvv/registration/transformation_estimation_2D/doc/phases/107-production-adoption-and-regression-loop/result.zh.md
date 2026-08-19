# Phase 107 Result: production-adoption-and-regression-loop

## 结论

本阶段按用户授权把有收益 RVV 实现接入当前 production patch，并对负向候选执行试接入后回退：

| 路径 | production 结果 | 证据决策 |
| --- | --- | --- |
| ordered-cloud-pair generic | 保留 / 接入 traits-gated PointXYZ-like public dispatch | Phase 107 board 16 cases 全部 positive；继续作为当前 adopted / retained production behavior。 |
| source-indexed exact | 保留 Phase 091 `PointXYZ -> PointXYZ` narrow dispatch | Phase 107 board 4K/64K/256K `4.157x / 4.814x / 4.615x`，Doctor `0/0/0`。 |
| source-indexed generic widening | 回退到 exact gate | Phase 106 20-run public representative variance 为 full-widening negative；Phase 103/104 仍是 guarded probe，不采纳。 |
| dual-indexed exact | 接入当前 production patch | Phase 107 family A/B 64K/256K positive，4K weak/negative caveat；保留为 production patch candidate，等待用户检查，不自动提交。 |
| correspondence exact | 试接入后退回 | Phase 107 family A/B 256K `4/20` below-1，overall negative；production dispatch 已移除。 |

当前 production header 不含 correspondence RVV dispatch。generic dual-indexed / correspondence /
source-indexed widening 均未采纳。

## 源码变更事实

生产源码：

```text
registration/include/pcl/registration/impl/transformation_estimation_2D.hpp
```

当前保留：

- ordered-cloud-pair：`Scalar=float`，source/target 分别满足 `RVVXYZAoSFloatLayout<PointT>`。
- source-indexed：exact `PointXYZ -> PointXYZ`，`Scalar=float`，valid source indices。
- dual-indexed：exact `PointXYZ -> PointXYZ`，`Scalar=float`，valid source / target indices。

当前不保留：

- source-indexed generic traits-gated production dispatch；
- correspondence production RVV dispatch；
- dual-indexed / correspondence generic point-type production dispatch。

测试源码：

```text
test-rvv/registration/transformation_estimation_2D/src/test_te2d.cpp
```

接入后首轮 `run_test_compare` 暴露 RVV side 3 个 correspondence family 测试失败。失败不是
correspondence dispatch 仍在，而是 ordered generic RVV 启用后，测试把 scalar iterator、
ordered RVV 和 dual-indexed RVV 三个不同规约顺序的生产路径强制要求 checksum 逐位相同。
本阶段将该断言改为生产 RVV 误差预算内一致，保留 matrix near 检查，移除跨 family checksum
硬等式。

## Correctness

命令：

```bash
make -C test-rvv/registration/transformation_estimation_2D run_test_compare
```

结果：

- Std：`84/84` pass。
- RVV：`84/84` pass。

该结果覆盖当前 production dispatch hit / fallback，以及已存在的 diagnostic candidate
correctness corpus。QEMU correctness 不提供性能结论。

## QEMU / asm / Evidence Doctor

| target | result |
| --- | --- |
| `record_qemu_production_public_state` | Doctor `0/0/0`；ordered generic production public symbols 可见。 |
| `record_qemu_source_indexed_public_state` | Doctor `0/0/0`；`production_public_source_indexed_boundary` 46 RVV lines。 |
| `record_qemu_dual_indexed_family_ab_state` | Doctor `0/0/0`；`production_public_dual_indexed_boundary` 50 RVV lines。 |

correspondence dispatch 已退回，因此本阶段没有把 correspondence QEMU public 作为当前 production
path-hit 证据刷新。

## Board evidence

| run label | scope | summary |
| --- | --- | --- |
| `generic_xyz_point_types_public_phase107_repeated` | ordered generic public | 16 cases 全部 positive，`B/A<1=0/5`；`PointXYZ->PointXYZ` 4K/64K/256K 为 `4.400x / 5.556x / 5.184x`；Doctor `0/4/0`。 |
| `source_indexed_public_phase107_repeated` | source-indexed exact public | 4K/64K/256K 为 `4.157x / 4.814x / 4.615x`，`B/A<1=0/5`；Doctor `0/0/0`。 |
| `dual_indexed_family_ab_phase107_repeated` | dual-indexed exact production-detail family A/B | direct/materialize B/A 为 4K `1.085x`、64K `1.691x`、256K `1.678x`；4K 有 `1/5` below-1；Doctor `0/3/0`。 |
| `correspondence_public_phase107_repeated` | correspondence exact trial public Std/RVV | public RVV 相对 public scalar 为 positive；只作为试接入证据，不能越过 family A/B negative。 |
| `correspondence_family_ab_phase107_repeated` | correspondence exact trial family A/B | 4K `1.091x`、64K `1.645x`、256K `1.385x`，但 256K `4/20` below-1；overall negative；Doctor `0/4/0`。 |

correspondence public positive 不能覆盖 family A/B negative；本阶段已按用户规则退回该 production
dispatch。dual-indexed 4K Warning 不阻塞 64K/256K 主规模收益，但必须保留 caveat。

Phase 107 board summary / manifest / Doctor 路径：

```text
test-rvv/registration/transformation_estimation_2D/log/board/generic_xyz_point_types_public_phase107_repeated/summary.md
test-rvv/registration/transformation_estimation_2D/log/board/generic_xyz_point_types_public_phase107_repeated/evidence_manifest.json
test-rvv/registration/transformation_estimation_2D/log/board/generic_xyz_point_types_public_phase107_repeated/evidence_doctor.md
test-rvv/registration/transformation_estimation_2D/log/board/source_indexed_public_phase107_repeated/summary.md
test-rvv/registration/transformation_estimation_2D/log/board/source_indexed_public_phase107_repeated/evidence_manifest.json
test-rvv/registration/transformation_estimation_2D/log/board/source_indexed_public_phase107_repeated/evidence_doctor.md
test-rvv/registration/transformation_estimation_2D/log/board/dual_indexed_family_ab_phase107_repeated/summary.md
test-rvv/registration/transformation_estimation_2D/log/board/dual_indexed_family_ab_phase107_repeated/evidence_manifest.json
test-rvv/registration/transformation_estimation_2D/log/board/dual_indexed_family_ab_phase107_repeated/evidence_doctor.md
test-rvv/registration/transformation_estimation_2D/log/board/correspondence_public_phase107_repeated/summary.md
test-rvv/registration/transformation_estimation_2D/log/board/correspondence_public_phase107_repeated/evidence_manifest.json
test-rvv/registration/transformation_estimation_2D/log/board/correspondence_public_phase107_repeated/evidence_doctor.md
test-rvv/registration/transformation_estimation_2D/log/board/correspondence_family_ab_phase107_repeated/summary.md
test-rvv/registration/transformation_estimation_2D/log/board/correspondence_family_ab_phase107_repeated/evidence_manifest.json
test-rvv/registration/transformation_estimation_2D/log/board/correspondence_family_ab_phase107_repeated/evidence_doctor.md
```

## Evidence Doctor 解释

- ordered generic board `0/4/0`：无 Error；Warnings 是长尾 / group outlier，所有 case
  仍为 positive 且 `B/A<1=0/5`，可作为当前 representative public performance evidence。
- dual-indexed board `0/3/0`：Warnings 全在 4K；64K/256K 稳定 positive。结论只覆盖 exact
  `PointXYZ -> PointXYZ`，不覆盖 generic point types。
- correspondence board `0/4/0`：Warnings 包含 256K 退化频率；因 decision bucket negative，
  不接入 production。

## 文档与 registry 同步

已同步：

- 本 result。
- `doc/phases/README.zh.md`。
- `doc/optimization-roadmap.zh.md`。
- `doc/phases/optimization-matrix.zh.md`。
- `doc/transformation_estimation_2D-evaluation.zh.md`。
- `doc-rvv/registration/transformation_estimation_2D-RVV.zh.md`。
- current handoff YAML / Markdown。
- `evidence_status` fresh。
- `git diff --check` pass。

## 停止条件

本阶段完成后停在用户检查点：

- 当前 patch 已接入 ordered generic、source-indexed exact、dual-indexed exact。
- source-indexed generic 和 correspondence direct 已按证据不采纳 / 退回。
- 不自动提交。
- 若用户确认 dual-indexed exact 采纳，可进入提交或最终 closeout；若希望更严格，也可追加
  dual-indexed 20-run 方差 phase。
