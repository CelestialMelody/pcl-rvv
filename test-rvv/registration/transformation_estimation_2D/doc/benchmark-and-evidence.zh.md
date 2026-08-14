# Benchmark 与证据说明

## 本文职责

本文记录 `transformation_estimation_2D` 的 bench（性能测试）入口、case-filter（用例过滤参数）、计时边界、QEMU / board 证据分层、asm attribution（反汇编归因）口径和提交边界。当前 production patch 保留在窄范围 ordered-cloud-pair public overload；Phase 030 的 row-source bench 仍是 test-only materialize-to-ordered 诊断。

## Bench 输出格式

`src/bench_te2d.cpp` 输出：

- topic banner 和 build 标记：Std 或 RVV。
- dataset：synthetic dense `PointXYZ` ordered-cloud-pair，或 synthetic dense row-source pairs。
- iterations 和 warmup iterations。
- 每个 case 的 `ms/iter`、`Total Time` 和 checksum（校验和）。

这些字段用于 smoke（小规模可运行性检查）和 board repeated summary 解析。QEMU 计时不进入性能排序。

## CLI 参数

| 参数 | 默认值 | 说明 |
| --- | --- | --- |
| `--case-filter` | all | `ordered-cloud-pair-public`、`ordered-cloud-pair-fused`、`row-source-fused` 或 `all`。 |
| `--iterations` | 20 | 测量循环次数。 |
| `--warmup-iterations` | 3 | warm-up（预热）次数。 |

## Bench Label / case-filter 字典

| case-filter | label | 边界 |
| --- | --- | --- |
| `ordered-cloud-pair-public` | `public 2D ordered-cloud-pair 4K/64K/256K` | 真实 public overload；当前 production patch 的直接性能入口。 |
| `ordered-cloud-pair-fused` | `fused 2D correlation ordered-cloud-pair 4K/64K/256K` | test-only fused candidate；RVV 构建下尝试 RVV 累加。 |
| `row-source-fused` | `fused 2D correlation source-indexed-cloud-pair`、`dual-indexed-cloud-pair`、`correspondence-pair`，各 4K/64K/256K | test-only materialize-to-ordered candidate；source / target 展开成本计入每次 case 计时。 |

## 推荐 Target

| target | 作用 | 证据等级 |
| --- | --- | --- |
| `run_bench_ordered_cloud_pair_smoke` | 只运行 RVV bench 的 fused ordered-cloud-pair 小规模 smoke。 | QEMU log shape only。 |
| `run_bench_ordered_cloud_pair_public_smoke` | 只运行 RVV bench 的 public ordered-cloud-pair 小规模 smoke。 | QEMU production-public probe log shape only。 |
| `run_bench_row_source_smoke` | 只运行 RVV bench 的三类 row-source candidate smoke，共 9 个 case。 | QEMU log shape only；不证明性能。 |
| `dump_bench_std` / `dump_bench_rvv` | 构建 Std / RVV bench 并导出 objdump。 | asm attribution input。 |
| `generate_asm_attribution_summary` | 生成 diagnostic asm summary。 | 反汇编归因摘要；不证明性能。 |
| `generate_production_public_asm_attribution_summary` | 生成 production-public probe asm summary。 | 路径命中和指令归属；不证明性能。 |
| `run_qemu_smoke_evidence_doctor` | 生成 QEMU diagnostic manifest 并运行 Evidence Doctor。 | QEMU 证据合同检查；不证明性能。 |
| `run_qemu_production_public_evidence_doctor` | 生成 QEMU production-public manifest 并运行 Evidence Doctor。 | QEMU probe 证据合同检查；不证明性能。 |
| `run_qemu_row_source_evidence_doctor` | 生成 row-source manifest 并运行 Evidence Doctor。 | 当前 9-case QEMU 证据合同检查；不证明性能。 |
| `run_board_bench_ordered_cloud_pair_repeated` | 部署 Std/RVV bench 到板卡并按 5-run 预算重复运行。 | pre-production board diagnostic summary。 |
| `run_board_bench_ordered_cloud_pair_public_repeated` | 部署当前 production patch 的 Std/RVV bench 到板卡并按 5-run 预算运行 public probe。 | production candidate board evidence。 |
| `run_board_bench_row_source_repeated` | 部署 Std/RVV bench 到板卡并按 5-run、20 iterations、5 warmup 预算运行 `row-source-fused`。 | row-source diagnostic board evidence；已完成，Doctor 异常使结论降级。 |

默认不在 QEMU 上运行完整 Std/RVV compare。QEMU 只用于 correctness、构建、路径命中和日志形状。

## 计时边界

bench case 在每次迭代内包含：

- 调用 public estimator 或 test-only fused candidate。
- 对 row-source candidate，包含 source / target index 或 correspondence 的 materialize-to-ordered 展开。
- 2D angle、`cos/sin` 和 4x4 matrix 写回。
- checksum 计算。

bench case 不包含：

- 输入点云构造。
- `makeRigid2DTransform` 和 `transformCloud2D`。
- 日志解析、Evidence Doctor 或 registry 更新。

## Checksum 来源

`matrixChecksum` 把 4x4 matrix 逐元素乘以 `1e6` 后转换为整数并做 FNV-like 混合。production-public bench log 的 checksum 还包含 RVV gate flag（路径标记），因此 Std / RVV log checksum 不要求相同。checksum 用于发现路径或输出明显变化，不替代逐元素 correctness 断言。

## 当前 QEMU 证据

Diagnostic smoke：

- `test-rvv/registration/transformation_estimation_2D/log/qemu/run_bench_ordered_cloud_pair_smoke_rvv.log`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/evidence_manifest.json`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/evidence_doctor.md`

Production-public smoke：

- `test-rvv/registration/transformation_estimation_2D/log/qemu/production_public/run_bench_ordered_cloud_pair_public_rvv.log`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/production_public/evidence_manifest.json`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/production_public/evidence_doctor.md`

Production-public QEMU Doctor 为 Errors=0、Warnings=0、Suggestions=0；manifest 中 `rvv_instr_count=52`。该结果只证明 probe binary 的路径和日志形状。

Row-source smoke：

- `test-rvv/registration/transformation_estimation_2D/log/qemu/row_source/run_bench_row_source_fused_rvv.log`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/row_source/evidence_manifest.json`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/row_source/evidence_doctor.md`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/row_source/asm_attribution.md`

该 manifest 覆盖 source-indexed、dual-indexed、correspondence 三类 row source，各 4K/64K/256K，共 9 个 comparison；Evidence Doctor 为 Errors=0、Warnings=0、Suggestions=0。它只证明可运行、日志字段和 wrapper / candidate 的归属形状。

## 当前 Board 证据

Diagnostic repeated summary：

| case | median B/A | min | max | bucket |
| --- | ---: | ---: | ---: | --- |
| `fused 2D correlation ordered-cloud-pair 4K` | 1.176x | 1.166x | 1.191x | `positive` |
| `fused 2D correlation ordered-cloud-pair 64K` | 1.096x | 1.068x | 1.145x | `weak_positive` |
| `fused 2D correlation ordered-cloud-pair 256K` | 1.096x | 1.080x | 1.121x | `weak_positive` |

该结果只覆盖 test-only fused candidate，证据角色是 pre-production diagnostic（接入生产前诊断）。它曾支持进入 PI1/PI2，不证明最终 production dispatch 成立。

Production-public repeated summary：

| case | median B/A | min | max | B/A<1 | bucket |
| --- | ---: | ---: | ---: | ---: | --- |
| `public 2D ordered-cloud-pair 4K` | 4.222x | 4.110x | 4.231x | 0/5 | `positive` |
| `public 2D ordered-cloud-pair 64K` | 5.310x | 5.199x | 5.362x | 0/5 | `positive` |
| `public 2D ordered-cloud-pair 256K` | 4.947x | 4.864x | 5.068x | 0/5 | `positive` |

Production-public summary 路径：

- `test-rvv/registration/transformation_estimation_2D/log/board/ordered_cloud_pair_public_repeated/summary.md`
- `test-rvv/registration/transformation_estimation_2D/log/board/ordered_cloud_pair_public_repeated/evidence_manifest.json`
- `test-rvv/registration/transformation_estimation_2D/log/board/ordered_cloud_pair_public_repeated/evidence_doctor.md`

该结果是当前 Phase 050 PI5 主证据，来源是最新真实 `Milkv-Jupiter` 板卡复跑。三个规模均为稳定 `positive`，支持保留窄范围 production patch。

Row-source board repeated 已使用仓库内 `test-rvv/config.mk`、`REMOTE_USER`、`REMOTE_IP` 和 `BOARD_LABEL=Milkv-Jupiter` 完成。当前 summary 见 `log/board/row_source_fused_repeated/summary.md`，共有 9 个 comparison；其中 correspondence 64K 有 2/5 退化，dual-indexed 64K 有 1/5 退化并出现长尾，因此只能作为降级的 test-only 诊断。

## Evidence Doctor / Manifest 边界

| evidence | Errors | Warnings | Suggestions | 处理 |
| --- | ---: | ---: | ---: | --- |
| diagnostic QEMU manifest | 0 | 0 | 0 | 只作为 QEMU smoke contract。 |
| diagnostic board manifest | 0 | 0 | 0 | 只作为 pre-production diagnostic。 |
| production-public QEMU manifest | 0 | 0 | 0 | 只作为 QEMU probe contract。 |
| row-source QEMU manifest | 0 | 0 | 0 | 只作为 9-case QEMU smoke contract。 |
| production-public board manifest | 0 | 0 | 0 | 支持窄范围 production candidate；仍需用户审阅 production diff。 |
| row-source board manifest | 1 | 2 | 6 | 保留为 test-only 诊断；不进入 production dispatch。 |

QEMU Doctor 的 `0/0/0` 不等于性能通过；性能结论只来自 board summary。Row-source Doctor 的 Error/Warning 对应 correspondence 64K、dual-indexed 64K 的退化频率和长尾，已写入 Phase 030 result。

## ASM Attribution 口径

Diagnostic asm 输出：

- `test-rvv/registration/transformation_estimation_2D/log/qemu/asm_attribution.md`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/asm_attribution.json`

Production-public asm 输出：

- `test-rvv/registration/transformation_estimation_2D/log/qemu/production_public/asm_attribution.md`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/production_public/asm_attribution.json`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/row_source/asm_attribution.md`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/row_source/asm_attribution.json`

Production-public summary 结论为 `production_public_inline_boundary_present_with_candidate_boundary_with_other_rvv_boundaries`。它能证明 probe binary 中 public overload 或 `runPublicCase` 内联边界出现关键 RVV 指令；它不能证明目标硬件性能成立。

Row-source summary 聚焦 `row_source_lambda_boundary`，当前结论为 `row_source_lambda_boundary_present_with_candidate_boundary_with_other_rvv_boundaries`。它能证明三类 wrapper lambda 的路径归属，但共享数学 RVV 仍归属于 test-support fixture，不能替代 production asm 或 board evidence。

## 复现命令

```bash
make -C test-rvv/registration/transformation_estimation_2D run_test_compare
make -C test-rvv/registration/transformation_estimation_2D run_bench_ordered_cloud_pair_smoke
make -C test-rvv/registration/transformation_estimation_2D generate_asm_attribution_summary
make -C test-rvv/registration/transformation_estimation_2D run_qemu_smoke_evidence_doctor
make -C test-rvv/registration/transformation_estimation_2D run_qemu_production_public_evidence_doctor
make -C test-rvv/registration/transformation_estimation_2D run_bench_row_source_smoke
make -C test-rvv/registration/transformation_estimation_2D run_qemu_row_source_evidence_doctor
make -C test-rvv/registration/transformation_estimation_2D record_board_ordered_cloud_pair_public_state
make -C test-rvv/registration/transformation_estimation_2D run_board_bench_row_source_repeated
make -C test-rvv/registration/transformation_estimation_2D evidence_status
```

`record_board_ordered_cloud_pair_public_state` 只解析并登记已抓回的 repeated board run；正式板卡采集入口是 `run_board_bench_ordered_cloud_pair_public_repeated`。
`run_board_bench_row_source_repeated` 使用 `test-rvv/config.mk` 中的板卡配置；配置存在且板卡可达时必须运行，不得以 QEMU timing 代替。

## 提交边界

默认提交源码 scaffold、Makefile、board.mk、topic-local script 和文档。`build/`、`log/qemu/*.log`、`log/board/**/run-*`、`log/evidence_registry.json` 和板卡 raw logs 都是本地生成产物，除非用户明确要求提交脱敏 evidence logs，否则不提交。

当前 summary-only 证据路径已被本文和 phase result 引用，用于 registry freshness（证据新鲜度）检查；它们是证据指针，不是 raw log 提交授权。
