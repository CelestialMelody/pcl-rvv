# Phase 013 Result：indexed direct gather point type / layout

## 当前结论

Phase 013 已完成。`source-indexed-cloud-pair` 和 `dual-indexed-cloud-pair` direct
gather 已新增 `PointXYZI` / `PointXYZRGB` correctness 覆盖，bench 新增
`indexed-direct-gather-point-type-layout` filter，并完成板端 5-run repeated
summary / manifest / Evidence Doctor（证据体检）登记。

本阶段没有修改 production TEDQ header。`registration/include/pcl/registration/impl/transformation_estimation_dual_quaternion.hpp`
保持无 diff。

## 动作回填

| action | 状态 | 证据 / 产物 | 结论 |
| --- | --- | --- | --- |
| A1 correctness | done | `make run_test_compare` | Std/RVV 各 `28/28 tests passed`；新增 source-indexed / dual-indexed × `PointXYZI` / `PointXYZRGB` direct gather 测试通过。 |
| A2 bench filter | done | `src/bench_tedq.cpp` | 新增 `indexed-direct-gather-point-type-layout`，输出 source/dual × point type × 4K/64K/256K。 |
| A3 QEMU smoke | done | `make record_qemu_smoke_evidence_state BENCH_ARGS="--iterations 2 --warmup-iterations 1 --case-filter indexed-direct-gather-point-type-layout"` | 12 个 smoke case 生成 manifest；Evidence Doctor `Errors=0 / Warnings=0 / Suggestions=0`。 |
| A4 board repeated | done | `make run_board_bench_indexed_direct_gather_point_type_layout_repeated` | 12 个 board case 全部 positive；Evidence Doctor `Errors=0 / Warnings=4 / Suggestions=0`。 |

## Phase 013 证据

- correctness：Std/RVV 各 `28/28 tests passed`。
- QEMU smoke case-filter：`indexed-direct-gather-point-type-layout`。
- QEMU smoke cases：source-indexed / dual-indexed × `PointXYZI` / `PointXYZRGB` × 4K / 64K / 256K。
- QEMU Evidence Doctor：`Errors=0 / Warnings=0 / Suggestions=0`。
- QEMU timing 只证明日志形状，不进入性能结论。
- board repeated case-filter：`indexed-direct-gather-point-type-layout`。
- board summary：`log/board/indexed_direct_gather_point_type_layout_repeated/summary.md`。
- board Evidence Doctor：`log/board/indexed_direct_gather_point_type_layout_repeated/evidence_doctor.md`，
  result 为 `Errors=0 / Warnings=4 / Suggestions=0`。

| row source policy | point type | 4K | 64K | 256K | decision |
| --- | --- | ---: | ---: | ---: | --- |
| `source-indexed-cloud-pair` | `PointXYZI` | `2.444x` | `2.798x` | `2.846x` | `positive` |
| `source-indexed-cloud-pair` | `PointXYZRGB` | `2.483x` | `2.866x` | `2.845x` | `positive` |
| `dual-indexed-cloud-pair` | `PointXYZI` | `2.370x` | `3.034x` | `4.628x` | `positive` |
| `dual-indexed-cloud-pair` | `PointXYZRGB` | `2.464x` | `3.028x` | `4.862x` | `positive` |

## Evidence Doctor 解释

四个 warning 都来自板端数值形态，不是 correctness error。`dual-indexed-cloud-pair`
的 `PointXYZRGB 4K` 有轻微 long-tail / variance（长尾 / 波动），`dual-indexed-cloud-pair`
的 `PointXYZI 4K`、`PointXYZI 256K` 和 `PointXYZRGB 256K` 是组内 outlier（离群）。
因此本阶段可写 `diagnostic_positive_with_warnings`，但必须按 row source、point type
和 size 分开读数，不能把某个规模或点型的收益外推到其它组合，也不能升级为 production
performance（生产性能）结论。

## 继续 / 停止决定

- 当前阶段：`diagnostic_positive_with_warnings`。
- 当前 topic：继续保持 `no-production`。
- 下一动作：当前 topic 已完成四类 row-source policy 的 test-rvv 诊断覆盖与代表性
  `PointXYZI` / `PointXYZRGB` point-type layout 扩展。若继续到生产，需要先进入
  `PI1-production-reentry-contract`，由用户明确判断是否授权修改 production 头文件。
