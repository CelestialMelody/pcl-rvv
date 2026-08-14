# Phase 050 Plan: pi2-production-patch-and-direct-evidence

## 阶段意图和边界

用户已授权继续推进 production integration loop（生产接入闭环）。本阶段按 Phase 040
冻结范围连续推进 PI2-PI5：写最小 production patch（生产补丁）、补 production direct
tests（真实生产路径测试）、生成 production asm attribution（生产反汇编归因）和 production
board evidence（板卡生产证据），再做 PI5 EvidenceDecision（证据决策）。

本阶段不得扩大 Phase 040 范围。

| 维度 | 本阶段范围 |
| --- | --- |
| production 文件 | `registration/include/pcl/registration/impl/transformation_estimation_2D.hpp` |
| public entry | `estimateRigidTransformation(cloud_src, cloud_tgt, matrix)` ordered-cloud-pair overload |
| row source policy | ordered-cloud-pair（顺序点云对，source/target 按相同下标一一对应） |
| 点型 / Scalar | exact（精确点型匹配）`pcl::PointXYZ -> pcl::PointXYZ`、`Scalar=float` |
| 输入 gate | 两侧 size 相等、非空、`n >= 16`、两侧 `is_dense == true`、两侧每个点 `pcl::isFinite` |
| fallback | 非 RVV build、`Scalar=double`、非 `PointXYZ`、indices、correspondences、小规模、非 dense、非有限输入 |
| 不做 | 不改 public API；不扩泛型点型；不扩 source-indexed、dual-indexed 或 correspondence-pair |

## 当前状态清单

| area | 当前状态 | 证据路径 |
| --- | --- | --- |
| Phase 040 | PI1 complete | `doc/phases/040-production-integration-plan/result.zh.md` |
| diagnostic correctness | Std / RVV 各 10 个 gtest 通过 | `log/qemu/run_test_std.log`、`log/qemu/run_test_rvv.log` |
| diagnostic asm | candidate lambda 边界含 `vlsseg3e32.v`、`vfmacc`、`vfredosum` | `log/qemu/asm_attribution.md` |
| diagnostic board | 5-run overall `weak_positive` | `log/board/ordered_cloud_pair_repeated/summary.md` |
| production source | 当前未改 | `registration/include/pcl/registration/impl/transformation_estimation_2D.hpp` |
| `doc-rvv` | not_applicable | PI5 通过前不创建 production 长期主题文档 |

## 实现计划

PI2 production patch：

1. 在 `__RVV10__` 下新增 `pcl::registration::detail` 内部 helper，返回 `bool` 表示是否命中。
2. helper 只在 `PointSource == pcl::PointXYZ`、`PointTarget == pcl::PointXYZ`、`Scalar == float` 时进入实现分支。
3. 运行期 gate 检查 size、dense、小规模和 finite scan。任一失败返回 `false`，由现有 iterator helper 标量路径处理。
4. RVV 路径复用 Phase 020 的两遍中心化融合 2D 相关项累加器：
   - 第一遍 RVV 累加 source / target 的 x/y sum。
   - 第二遍 RVV 累加 `H00/H01/H10/H11`。
   - `atan2`、`cos/sin` 和 4x4 matrix 写回保持标量。
5. public ordered overload 在 size mismatch 检查之后短路调用 helper；成功则 `return`，失败继续原有 iterator path。

## 测试和证据计划

| id | 动作 | 产物 / 命令 | 完成判据 |
| --- | --- | --- | --- |
| PI2-A1 | 写 production helper 和 public dispatch | `transformation_estimation_2D.hpp` | public API 不变；非 RVV build 不含 helper；未覆盖范围返回 false。 |
| PI3-A1 | production direct correctness | `src/test_te2d.cpp`、`run_test_compare` | public ordered overload 在 Std / RVV 构建下通过；RVV build 直接调用 helper 命中 exact gate。 |
| PI3-A2 | fallback tests | `src/test_te2d.cpp` | 小规模、非 dense、非有限、`Scalar=double`、`PointXYZI`、indices、correspondences 均保持标量边界。 |
| PI4-A1 | QEMU production public smoke | `run_bench_ordered_cloud_pair_public_smoke` | 只检查日志形状和 production asm 输入，不写 QEMU 性能结论。 |
| PI4-A2 | production asm attribution | `generate_asm_attribution_summary` | 能从 production helper / public boundary 区分 test-support fused candidate。 |
| PI4-A3 | board production repeated | `run_board_bench_ordered_cloud_pair_public_repeated` | 5-run public Std/RVV summary、manifest、Evidence Doctor 和 registry 记录。 |
| PI5-A1 | EvidenceDecision | phase result、matrix、roadmap、evaluation、适用 `doc-rvv` | production direct correctness、fallback、asm、board 和 doctor 闭合后才写 adopted。 |

## Evidence Doctor 和 Registry

本阶段新增 production public evidence，不能复用 Phase 020 的 diagnostic manifest。计划新增：

```text
log/qemu/production_public/run_bench_ordered_cloud_pair_public_rvv.log
log/qemu/production_public/evidence_manifest.json
log/qemu/production_public/evidence_doctor.md
log/board/ordered_cloud_pair_public_repeated/summary.md
log/board/ordered_cloud_pair_public_repeated/evidence_manifest.json
log/board/ordered_cloud_pair_public_repeated/evidence_doctor.md
```

Registry labels：

```text
qemu-te2d-production-public-smoke-pi4
board-te2d-production-public-repeated-pi4
```

QEMU role 为 `qemu_smoke_only`；board role 为 `post_production_performance`。

## Board 复跑预算和决策桶

| 项 | 计划值 |
| --- | --- |
| run count | 5 |
| iterations | 20 |
| warm-up iterations | 5 |
| case-filter | `ordered-cloud-pair-public` |
| 追加预算 | 最多 1 次同边界 5-run；仅在 bucket 摇摆或 Evidence Doctor 要求时使用。 |

决策桶沿用 Phase 040：`positive` / `weak_positive` 可进入 PI5 production-ready 判断；
`neutral`、`negative` 或 `unstable` 触发降级或回退判断。

## Optimization Matrix

| candidate family | row source policy | point type / Scalar / layout | correctness / fallback target | bench / board target | asm boundary | Evidence Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- |
| production dispatch | ordered-cloud-pair | exact `PointXYZ -> PointXYZ`, `Scalar=float`, dense finite AoS | planned PI3 production direct + fallback tests | planned `ordered-cloud-pair-public` repeated | planned production helper / public boundary attribution | planned QEMU + board doctor | in_progress |
| generic xyz point types | ordered-cloud-pair | traits-gated xyz AoS | fallback tests only | not_run | not_run | not_run | deferred |
| indexed / correspondences | non-ordered row sources | query / match or index row source | fallback public semantics tests | not_run | not_run | not_run | fallback in this phase |

## 暂停条件

- production helper 需要修改 public / protected API。
- exact gate 以外的点型、`Scalar` 或 row source 才能让补丁成立。
- fallback gate 无法单独测试。
- production direct correctness 与 Phase 020 诊断结果矛盾。
- asm attribution 不能归到 production helper / public boundary。
- board 不可达、Evidence Doctor Error 未修复，或 production board bucket 为 negative / unstable。

## 文档更新清单

- `doc/phases/050-pi2-production-patch-and-direct-evidence/result.zh.md`
- `doc/phases/README.zh.md`
- `doc/phases/optimization-matrix.zh.md`
- `doc/optimization-roadmap.zh.md`
- `doc/transformation_estimation_2D-evaluation.zh.md`
- `doc/optimization-evidence.zh.md`
- `doc/benchmark-and-evidence.zh.md`
- `doc/testing-overview.zh.md`
- `doc/correctness-tests.zh.md`
- `doc/test-support-code-map.zh.md`
- `README.zh.md`
- 当前 Handoff Packet

PI5 通过后创建 `doc-rvv/registration/transformation_estimation_2D-RVV.zh.md`；若 PI5 不通过，继续保持 not_applicable。

## Continue / Stop 条件

`continue_stop_decision`：用户已授权 production patch，本阶段默认继续到 PI5，除非命中暂停条件。

`next_phase_default`：本阶段完成后，若 production direct evidence 通过，则进入 `030-row-source-family-carryover`；
若 production direct evidence 不成立，则进入 no-production / rollback closeout。
