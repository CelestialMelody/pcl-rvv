# Benchmark 与证据说明

## 本文职责

本文说明 `src/bench_tedq.cpp` 的输出格式、case-filter 字典、计时边界、Evidence Doctor manifest
和提交边界。QEMU timing（QEMU 计时）只作为 smoke，不写性能结论。

## Bench 输出格式

bench 输出包含 `Dataset`、`Build`、`Iterations`、`Warmup Iterations`、每个 case 的
`ms/iter`、`Total Time` 和 `checksum`。Std/RVV 严格性能结论只来自目标板卡 repeated。

## Case-filter 字典

| filter | case label | 计时边界 | 证明点 | 不能证明 |
| --- | --- | --- | --- | --- |
| `production-public-retained-row-sources` | 三类 `public dual quaternion ...` retained row-source labels | 当前 production public entry，含 dispatch gate、C1/C2 累加和 Eigen 4x4 solve | retained 三类 production direct 同边界 Std/RVV 对比 | correspondence production RVV、QEMU 性能、跨 row-source 外推 |
| `production-public-row-sources` | Phase 015 四类 public labels | 历史 production probe | 说明 correspondence production RVV 为负向历史证据 | 当前 retained production patch |
| `public-dual-quaternion` | ordered public labels | 早期 ordered production probe / public baseline | Phase 002 历史 neutral | 当前 retained 三类 evidence |
| `component-ablation` | component accum only / solve only | test-support component strict A/B | 判断 C1/C2 前端和 solve 后段成本 | production direct |
| `row-source-expansion` | staged source / dual / correspondence labels | row 展开、staging、C1/C2 和 solve | row-source staged diagnostic | direct gather 或 production dispatch |
| `indexed-direct-gather-family-comparison` | staged vs direct gather labels | 同一 RVV binary 内 implementation-family A/B | source / dual / correspondence direct gather 诊断 | production direct |
| `correspondence-*` filters | direct index stream、segment-load、locality、point-type layout | test-only correspondence candidates | correspondence 专项诊断 | 当前 production patch |

## 当前 QEMU 证据

默认 smoke：

```bash
make run_qemu_smoke_evidence_doctor
```

默认 `QEMU_SMOKE_BENCH_ARGS` 为
`--case-filter production-public-retained-row-sources --iterations 1 --warmup-iterations 0`。

证据路径：

- `log/qemu/run_bench_std.log`
- `log/qemu/run_bench_rvv.log`
- `log/qemu/evidence_manifest.json`
- `log/qemu/evidence_doctor.md`

Evidence Doctor result：Errors=0，Warnings=9，Suggestions=0。所有 warning 均为
`zero_warmup_iterations`，因此这批数据只作为 log-shape / checksum smoke，不参与 speedup 结论。

## 当前 Board Production Evidence

Phase 016 retained-only board repeated：

```bash
make run_board_bench_production_public_repeated
```

采集合同：

- device：`Milkv-Jupiter`
- case-filter：`production-public-retained-row-sources`
- repeated runs：`5`
- iterations：`20`
- warm-up iterations：`5`
- B/A：`Std public ms / RVV public ms`

| row source policy | summary | 4K | 64K | 256K | doctor |
| --- | --- | ---: | ---: | ---: | --- |
| `ordered-cloud-pair` | `log/board/production_public_retained_row_sources_repeated/summary.md` | `3.232x` | `3.644x` | `3.656x` | `0/0/0` |
| `source-indexed-cloud-pair` | `log/board/production_public_source_indexed_cloud_pair_repeated/summary.md` | `2.540x` | `2.631x` | `2.586x` | `0/0/0` |
| `dual-indexed-cloud-pair` | `log/board/production_public_dual_indexed_cloud_pair_repeated/summary.md` | `2.015x` | `1.808x` | `2.021x` | `0/0/0` |

该证据支持三类 retained production candidate（保留的生产候选）进入用户提交判断。
`correspondence-pair` 不属于 retained case-filter。

## Historical Correspondence Evidence

Phase 015 的 `production-public-row-sources` 曾覆盖 correspondence production RVV：
4K `2.443x`，64K `0.746x`，256K `0.867x`，Evidence Doctor `2/3/0`。
Phase 016 已移除该 production dispatch。该证据只说明当前 correspondence production RVV 不保留，
不能用于否定 test-rvv correspondence 诊断候选。

## ASM Attribution 口径

本轮运行：

```bash
make dump_bench_rvv
```

本地 asm dump 在 `build/asm/riscv/bench_transformation_estimation_dual_quaternion_rvv.asm`。
它用于确认 retained bench binary 中有 `vlse32`、`vluxei32`、`vfwcvt`、`vfredosum` 等 RVV 指令。
性能结论仍以 board repeated 和 Evidence Doctor 为准。

## 提交边界

默认可 review 的 summary evidence 是 registry、manifest、doctor 和 retained board summaries。
`log/qemu/*.log` 与 `log/board/**/run-*/*.log` 是 ignored-local raw logs；`build/`、远端路径和本机
`config.mk` 不进入默认提交边界。

## Evidence Registry 路径白名单

以下完整仓库相对路径用于 `make evidence_status` 的 `doc_ref` 检查。它们不改变当前证据角色；
historical / diagnostic 路径只作为可复核背景，retained production 仍只看三类 retained summary。

- `test-rvv/registration/transformation_estimation_dual_quaternion/log/board/correspondence_direct_index_stream_repeated/summary.md`
- `test-rvv/registration/transformation_estimation_dual_quaternion/log/board/correspondence_direct_index_stream_repeated/evidence_manifest.json`
- `test-rvv/registration/transformation_estimation_dual_quaternion/log/board/correspondence_direct_index_stream_repeated/evidence_doctor.md`
- `test-rvv/registration/transformation_estimation_dual_quaternion/log/board/correspondence_index_locality_ablation_repeated/summary.md`
- `test-rvv/registration/transformation_estimation_dual_quaternion/log/board/correspondence_index_locality_ablation_repeated/evidence_manifest.json`
- `test-rvv/registration/transformation_estimation_dual_quaternion/log/board/correspondence_index_locality_ablation_repeated/evidence_doctor.md`
- `test-rvv/registration/transformation_estimation_dual_quaternion/log/board/correspondence_point_type_layout_repeated/summary.md`
- `test-rvv/registration/transformation_estimation_dual_quaternion/log/board/correspondence_point_type_layout_repeated/evidence_manifest.json`
- `test-rvv/registration/transformation_estimation_dual_quaternion/log/board/correspondence_point_type_layout_repeated/evidence_doctor.md`
- `test-rvv/registration/transformation_estimation_dual_quaternion/log/board/correspondence_segment_load_repeated/summary.md`
- `test-rvv/registration/transformation_estimation_dual_quaternion/log/board/correspondence_segment_load_repeated/evidence_manifest.json`
- `test-rvv/registration/transformation_estimation_dual_quaternion/log/board/correspondence_segment_load_repeated/evidence_doctor.md`
- `test-rvv/registration/transformation_estimation_dual_quaternion/log/board/indexed_direct_gather_family_comparison_repeated/summary.md`
- `test-rvv/registration/transformation_estimation_dual_quaternion/log/board/indexed_direct_gather_family_comparison_repeated/evidence_manifest.json`
- `test-rvv/registration/transformation_estimation_dual_quaternion/log/board/indexed_direct_gather_family_comparison_repeated/evidence_doctor.md`
- `test-rvv/registration/transformation_estimation_dual_quaternion/log/board/indexed_direct_gather_point_type_layout_repeated/summary.md`
- `test-rvv/registration/transformation_estimation_dual_quaternion/log/board/indexed_direct_gather_point_type_layout_repeated/evidence_manifest.json`
- `test-rvv/registration/transformation_estimation_dual_quaternion/log/board/indexed_direct_gather_point_type_layout_repeated/evidence_doctor.md`
- `test-rvv/registration/transformation_estimation_dual_quaternion/log/board/production_public_correspondence_pair_repeated/summary.md`
- `test-rvv/registration/transformation_estimation_dual_quaternion/log/board/production_public_correspondence_pair_repeated/evidence_manifest.json`
- `test-rvv/registration/transformation_estimation_dual_quaternion/log/board/production_public_correspondence_pair_repeated/evidence_doctor.md`
- `test-rvv/registration/transformation_estimation_dual_quaternion/log/board/production_public_dual_indexed_cloud_pair_repeated/summary.md`
- `test-rvv/registration/transformation_estimation_dual_quaternion/log/board/production_public_dual_indexed_cloud_pair_repeated/evidence_manifest.json`
- `test-rvv/registration/transformation_estimation_dual_quaternion/log/board/production_public_dual_indexed_cloud_pair_repeated/evidence_doctor.md`
- `test-rvv/registration/transformation_estimation_dual_quaternion/log/board/production_public_retained_row_sources_repeated/summary.md`
- `test-rvv/registration/transformation_estimation_dual_quaternion/log/board/production_public_retained_row_sources_repeated/evidence_manifest.json`
- `test-rvv/registration/transformation_estimation_dual_quaternion/log/board/production_public_retained_row_sources_repeated/evidence_doctor.md`
- `test-rvv/registration/transformation_estimation_dual_quaternion/log/board/production_public_row_sources_repeated/summary.md`
- `test-rvv/registration/transformation_estimation_dual_quaternion/log/board/production_public_row_sources_repeated/evidence_manifest.json`
- `test-rvv/registration/transformation_estimation_dual_quaternion/log/board/production_public_row_sources_repeated/evidence_doctor.md`
- `test-rvv/registration/transformation_estimation_dual_quaternion/log/board/production_public_source_indexed_cloud_pair_repeated/summary.md`
- `test-rvv/registration/transformation_estimation_dual_quaternion/log/board/production_public_source_indexed_cloud_pair_repeated/evidence_manifest.json`
- `test-rvv/registration/transformation_estimation_dual_quaternion/log/board/production_public_source_indexed_cloud_pair_repeated/evidence_doctor.md`
- `test-rvv/registration/transformation_estimation_dual_quaternion/log/board/row_source_expansion_correspondence_repeated/summary.md`
- `test-rvv/registration/transformation_estimation_dual_quaternion/log/board/row_source_expansion_correspondence_repeated/evidence_manifest.json`
- `test-rvv/registration/transformation_estimation_dual_quaternion/log/board/row_source_expansion_correspondence_repeated/evidence_doctor.md`
- `test-rvv/registration/transformation_estimation_dual_quaternion/log/board/row_source_expansion_dual_indexed_repeated/summary.md`
- `test-rvv/registration/transformation_estimation_dual_quaternion/log/board/row_source_expansion_dual_indexed_repeated/evidence_manifest.json`
- `test-rvv/registration/transformation_estimation_dual_quaternion/log/board/row_source_expansion_dual_indexed_repeated/evidence_doctor.md`
- `test-rvv/registration/transformation_estimation_dual_quaternion/log/board/row_source_expansion_repeated/summary.md`
- `test-rvv/registration/transformation_estimation_dual_quaternion/log/board/row_source_expansion_repeated/evidence_manifest.json`
- `test-rvv/registration/transformation_estimation_dual_quaternion/log/board/row_source_expansion_repeated/evidence_doctor.md`
- `test-rvv/registration/transformation_estimation_dual_quaternion/log/board/row_source_family_comparison_repeated/summary.md`
- `test-rvv/registration/transformation_estimation_dual_quaternion/log/board/row_source_family_comparison_repeated/evidence_manifest.json`
- `test-rvv/registration/transformation_estimation_dual_quaternion/log/board/row_source_family_comparison_repeated/evidence_doctor.md`
