# Benchmark 与证据说明

## 本文职责

本文记录 bench（性能测试）入口、case-filter（用例过滤条件）、计时边界、checksum（校验和）来源、QEMU smoke（QEMU 小型验证）、board repeated summary（板卡重复摘要）、Evidence Doctor（证据体检）、反汇编归因和证据提交边界。当前 production（生产源码）已回滚，本文件只把 `production-direct` 作为 historical production direct probe（历史真实生产入口探针）证据来解释。

## CLI 参数和 Target 字典

| target / 参数 | 后端 | 默认用途 | 输出 |
| --- | --- | --- | --- |
| `--case-filter edge-batch` | QEMU / board | 预构造 squared distance（平方距离）数组上的 edge predicate（边判断）诊断 | `log/qemu/analyze_bench_compare_edge_batch.log`、`log/board/edge_batch_repeated/summary.md` |
| `--case-filter edge-gather-staging` | QEMU / board | correspondence index gather（对应关系索引离散加载）和 staging（暂存）诊断 | `log/qemu/analyze_bench_compare_edge_gather_staging.log`、`log/board/edge_gather_staging_repeated/summary.md` |
| `--case-filter acceptance-filter` | QEMU / board | accept rate（接受率）和保序 append 前的筛选诊断 | `log/qemu/analyze_bench_compare_acceptance.log`、`log/board/acceptance_filter_confirm/summary.md` |
| `--case-filter full-entry` | QEMU smoke | 固定 seed public entry（公开入口）可运行性 | 裸 analyze log 可再生成，当前不作为性能证据 |
| `--case-filter production-direct` | QEMU / board guarded target | 仅在临时生产探针补丁存在时证明真实 production dispatch | `log/board/production_direct_repeated/summary.md`、`evidence_doctor.md` |
| `CRPOLY_BENCH_ITERATIONS` | QEMU smoke 参数 | 控制本地 bench 迭代数 | 只影响日志形状和 checksum smoke |
| `CRPOLY_BOARD_BENCH_ITERATIONS`、`CRPOLY_BOARD_REPEATED_RUNS` | board 参数 | 控制目标硬件重复采集 | repeated summary 的 run budget 来源 |

## Bench case 字典

| case-filter | case label | 输入规模 | 计时边界 | checksum 来源 | 证据角色 |
| --- | --- | --- | --- | --- | --- |
| `edge-batch` | `edge-batch candidate 64K`、`edge-batch candidate 256K` | 64K / 256K 预构造 squared distance 数组 | edge similarity candidate + checksum | accepted mask 的 FNV-1a | QEMU log-shape / board diagnostic |
| `edge-gather-staging` | `edge-gather-staging candidate 64K`、`edge-gather-staging candidate 256K` | 64K / 256K edge pairs + `PointXYZ` clouds | correspondence index gather + squared distance staging + RVV edge formula + checksum | accepted mask 的 FNV-1a | QEMU log-shape / production-shaped board diagnostic |
| `acceptance-filter` | `accept-rate filter candidate 64K`、`accept-rate filter candidate 256K` | 64K / 256K counter 数组 | accept rate + filter + checksum | scaled rates 和 kept indices 的 FNV-1a | QEMU log-shape / board diagnostic |
| `full-entry` | `fixed-seed public entry 512 correspondences` | 512 条 correspondences，512 轮 rejection | 完整 production class 调用 + checksum | kept query indices 的 FNV-1a | public-entry-shaped smoke |
| `production-direct` | `production-direct public entry 2048 correspondences`、`production-direct public entry 8192 correspondences` | 2048 / 8192 条 identity correspondences，512 轮 rejection | 回滚前临时 production dispatch（生产分流）真实公开入口 + checksum | kept query/match indices 的 FNV-1a | historical production direct probe；当前默认开关关闭 |

## `production-direct` 开关语义

`CRPOLY_ENABLE_PRODUCTION_DIRECT_PROBE=1` 只允许 Makefile 执行 `production-direct` 历史探针 target。它不是编译宏，不会改写生产 header，不会自动启用 RVV helper，也不会恢复 Phase 030 已回滚的生产补丁。

`production-direct` bench 的 C++ 入口仍是生产类公开调用：`rejector.getRemainingCorrespondences(correspondences, remaining)`。因此证据含义由运行时工作区中的生产源码决定：

- 临时生产 dispatch 补丁存在时，Std build 走 `Standard`，RVV build 可命中 production RVV helper；此时 repeated board 结果可以作为 production direct 探针证据。
- 当前 rollback/no-production 状态下，目标生产文件无本 topic diff；即使设置该变量，bench 也只是测当前标量公开入口，不能证明生产 RVV 接入。

这个保护门的目的，是防止回滚后误跑历史 target，并把无 RVV dispatch 的结果误登记为 production RVV evidence。

## 当前 QEMU 证据

| 输出 | 状态 | 说明 |
| --- | --- | --- |
| `log/qemu/analyze_bench_compare_edge_batch.log` | generated | QEMU smoke 可解析。Std / RVV timing 只记录日志形状。 |
| `log/qemu/analyze_bench_compare_edge_gather_staging.log` | generated | QEMU smoke 可解析。Std / RVV timing 只记录日志形状。 |
| `log/qemu/analyze_bench_compare_acceptance.log` | generated | QEMU smoke 可解析。Std / RVV timing 只记录日志形状。 |
| `log/qemu/edge_batch/evidence_manifest.json` | generated | topic-local wrapper 生成 manifest。 |
| `log/qemu/edge_gather_staging/evidence_manifest.json` | generated | topic-local wrapper 生成 production-shaped diagnostic manifest。 |
| `log/qemu/acceptance_filter/evidence_manifest.json` | generated | topic-local wrapper 生成 manifest。 |
| `log/qemu/production_direct/evidence_manifest.json` | historical | 回滚前 production-direct smoke 的 manifest；当前只作为负向探针归档输入。 |
| `log/qemu/edge_batch/evidence_doctor.md` | pass | Errors=0，Warnings=0，Suggestions=0。 |
| `log/qemu/edge_gather_staging/evidence_doctor.md` | pass | Errors=0，Warnings=0，Suggestions=0。 |
| `log/qemu/acceptance_filter/evidence_doctor.md` | pass | Errors=0，Warnings=0，Suggestions=0。 |
| `log/qemu/production_direct/evidence_doctor.md` | historical pass | Errors=0，Warnings=0，Suggestions=0；QEMU 只证明日志形状和路径合同。 |

QEMU timing 不能进入 performance evidence（性能证据）。当前日志中的 RVV/Std 数字只说明二进制可运行、输出合同可解析和 Evidence Doctor 输入完整。

## 当前 Board 证据

| 输出 | decision_bucket | 关键结果 | Evidence Doctor | 结论边界 |
| --- | --- | --- | --- | --- |
| `log/board/test_smoke/run_test.log` | not_applicable | 8 tests passed | manual correctness check | 证明 board 上当前 correctness 二进制可运行。 |
| `log/board/edge_batch_repeated/summary.md` | `weak_positive` | 64K median 1.069x；256K median 1.057x；degradation 0/5 | Errors=0，Warnings=0，Suggestions=0 | 只证明预构造距离数组局部公式弱正向。 |
| `log/board/edge_gather_staging_repeated/summary.md` | `weak_positive` | 64K median 1.100x；256K median 1.119x；degradation 0/5 | Errors=0，Warnings=0，Suggestions=0 | 证明 correspondence index gather + squared distance staging 后局部公式仍弱正向；不是 production direct。 |
| `log/board/acceptance_filter_repeated/summary.md` | `neutral` | 256K median 1.009x | Errors=0，Warnings=0，Suggestions=1 | 触发确认复跑。 |
| `log/board/acceptance_filter_confirm/summary.md` | `neutral` | 256K median 1.018x；degradation 1/5 | Errors=0，Warnings=1，Suggestions=1 | `accept_rate_filter` 保持 no-production。 |
| `log/board/production_direct_repeated/summary.md` | `negative` | 2048 median 0.901x；8192 median 0.956x；两组 degradation 5/5 | Errors=2，Warnings=0，Suggestions=0 | 回滚前真实公开入口生产探针负向；用于 `rollback/no-production`，不代表当前 production 仍有 RVV dispatch。 |

## 计时边界

| case-filter | Std / RVV 可比性 | 计时内包含 | 计时外或不能证明 |
| --- | --- | --- | --- |
| `edge-batch` | 同一 wrapper、同一预构造距离数组、同一 checksum policy | edge similarity candidate 和 checksum 生成 | point cloud gather、random sampling、histogram / Otsu |
| `edge-gather-staging` | 同一 wrapper、同一 PointXYZ clouds、同一 edge pairs | index 展开、点云读取、squared distance staging、edge predicate、checksum | 完整 `getRemainingCorrespondences` 的采样和输出 append |
| `acceptance-filter` | 同一 counter 数组和同一 threshold | accept rate 计算、filter mask、kept index checksum | histogram 生成、Otsu cut 和 production 容器状态 |
| `full-entry` | reference / production public entry 形态 smoke | 生产类公开入口、固定 seed rejection、checksum | 未接 RVV production dispatch，不能形成生产性能结论 |
| `production-direct` | 回滚前临时生产探针下的 strict A/B | public entry dispatch、RVV helper 或 Standard fallback、完整 rejection、checksum | 当前回滚状态下再次运行只能测标量公开入口 |

## Checksum 来源

checksum 使用 FNV-1a 风格摘要，目标是发现输出序列、mask 或 kept indices 的不一致。它不是 numerical correctness（数值正确性）证明；正确性仍由 gtest 对 reference path 与 production public entry 做逐项断言。

| case-filter | checksum 输入 | 说明 |
| --- | --- | --- |
| `edge-batch`、`edge-gather-staging` | accepted mask | mask 顺序变化会改变 checksum |
| `acceptance-filter` | scaled rates 和 kept indices | 同时覆盖 rate 数值和输出保序 |
| `full-entry` | kept query indices | 固定 seed 后用于 smoke 输出一致性 |
| `production-direct` | kept query/match indices | 回滚前探针用于 strict A/B 的输出指纹 |

## 反汇编证据

`dump_bench_rvv` 生成：

- `build/asm/riscv/bench_correspondence_rejection_poly_rvv.full.asm`
- `build/asm/riscv/bench_correspondence_rejection_poly_rvv.asm`

摘要中包含 `vfmin.vv`、`vfmax.vv`、`vfdiv.vv`、`vmfge.vf`、`vmsne.vi`、`vmerge.vvm`、`vse32.v`。诊断候选 helper 被编译器内联到 bench lambda，符号级 hot path attribution（热点路径归属）未完全闭合。

Phase 030 的完整反汇编 `build/asm/riscv/bench_correspondence_rejection_poly_rvv.full.asm` 在回滚前包含 `getRemainingCorrespondencesRVV` 和 `getRemainingCorrespondencesStandard`。这只归属历史 production probe，当前 production 源码已经恢复为标量路径。

## Evidence registry

当前 registry 是 `log/evidence_registry.json`。它登记 correctness logs、QEMU summary、manifest、doctor report、board summary、board doctor、asm summary 和 historical production-direct probe。raw `run_bench_*.log` 默认本机保留，不进入提交候选，除非用户明确要求日志并完成脱敏检查。

`production-direct` 运行入口默认被 `CRPOLY_ENABLE_PRODUCTION_DIRECT_PROBE=1` 保护。当前 no-production closeout 下只允许读取既有历史摘要；重新采集“生产 RVV 接入”证据必须先临时应用生产探针补丁并显式打开该开关。只打开开关而不恢复生产 dispatch，不能形成 production RVV evidence。

## 登记路径白名单

以下路径使用完整仓库相对路径书写，供 evidence registry（证据登记表）检查文档引用。QEMU bench compare 仍只是 log-shape smoke（日志形状验证），不能作为性能结论。

| 路径 | 角色 |
| --- | --- |
| `test-rvv/registration/correspondence_rejection_poly/log/qemu/analyze_bench_compare_edge_batch.log` | edge-batch QEMU log-shape smoke。 |
| `test-rvv/registration/correspondence_rejection_poly/log/qemu/edge_batch/evidence_manifest.json` | edge-batch QEMU Evidence Doctor 输入。 |
| `test-rvv/registration/correspondence_rejection_poly/log/qemu/edge_batch/evidence_doctor.md` | edge-batch QEMU Evidence Doctor 输出。 |
| `test-rvv/registration/correspondence_rejection_poly/log/qemu/analyze_bench_compare_edge_gather_staging.log` | edge-gather-staging QEMU log-shape smoke。 |
| `test-rvv/registration/correspondence_rejection_poly/log/qemu/edge_gather_staging/evidence_manifest.json` | edge-gather-staging QEMU Evidence Doctor 输入。 |
| `test-rvv/registration/correspondence_rejection_poly/log/qemu/edge_gather_staging/evidence_doctor.md` | edge-gather-staging QEMU Evidence Doctor 输出。 |
| `test-rvv/registration/correspondence_rejection_poly/log/qemu/analyze_bench_compare_acceptance.log` | acceptance-filter QEMU log-shape smoke。 |
| `test-rvv/registration/correspondence_rejection_poly/log/qemu/acceptance_filter/evidence_manifest.json` | acceptance-filter QEMU Evidence Doctor 输入。 |
| `test-rvv/registration/correspondence_rejection_poly/log/qemu/acceptance_filter/evidence_doctor.md` | acceptance-filter QEMU Evidence Doctor 输出。 |
| `test-rvv/registration/correspondence_rejection_poly/build/asm/riscv/bench_correspondence_rejection_poly_rvv.asm` | asm summary（反汇编摘要）路径。 |
| `test-rvv/registration/correspondence_rejection_poly/build/asm/riscv/bench_correspondence_rejection_poly_rvv.full.asm` | historical production probe 完整反汇编路径。 |
| `test-rvv/registration/correspondence_rejection_poly/script/generate_crpoly_evidence_manifest.py` | topic-local QEMU manifest wrapper。 |
| `test-rvv/registration/correspondence_rejection_poly/script/summarize_crpoly_board_repeated.py` | topic-local board repeated summary / manifest wrapper。 |

## Evidence Doctor 处理边界

| 输入 | 结果 | 处理动作 |
| --- | --- | --- |
| `log/qemu/edge_batch/evidence_doctor.md` | Errors=0，Warnings=0，Suggestions=0 | 只支持 QEMU log-shape 和 manifest 合同。 |
| `log/qemu/edge_gather_staging/evidence_doctor.md` | Errors=0，Warnings=0，Suggestions=0 | 只支持 QEMU log-shape 和 manifest 合同。 |
| `log/qemu/acceptance_filter/evidence_doctor.md` | Errors=0，Warnings=0，Suggestions=0 | 只支持 QEMU log-shape 和 manifest 合同。 |
| `log/qemu/production_direct/evidence_doctor.md` | Errors=0，Warnings=0，Suggestions=0 | historical probe 的 QEMU 证据，不能支撑性能。 |
| `log/board/edge_gather_staging_repeated/evidence_doctor.md` | Errors=0，Warnings=0，Suggestions=0 | 保留为 production-shaped diagnostic 历史正向线索。 |
| `log/board/production_direct_repeated/evidence_doctor.md` | Errors=2，Warnings=0，Suggestions=0 | 生产采用失败；生产补丁回滚，EvidenceDecision 为 `rollback/no-production`。 |

## 复现命令

```bash
make -C test-rvv/registration/correspondence_rejection_poly run_test_compare
make -C test-rvv/registration/correspondence_rejection_poly run_board_test_smoke
make -C test-rvv/registration/correspondence_rejection_poly run_bench_qemu_smoke
make -C test-rvv/registration/correspondence_rejection_poly run_board_bench_repeated
```

历史 `production-direct` target 需要额外开关：

```bash
CRPOLY_ENABLE_PRODUCTION_DIRECT_PROBE=1 make -C test-rvv/registration/correspondence_rejection_poly run_board_bench_production_direct_repeated
```

该命令只适合临时生产探针补丁已经存在的工作区。当前回滚状态下运行它不会恢复 RVV production dispatch。

## 提交边界

summary artifact、manifest 和 Evidence Doctor 报告在被 README、evaluation、roadmap、phase result 或 Handoff 明确引用时可进入 summary-only 审查候选。raw board run 子目录、完整 `run_bench_*.log`、本机 `config.mk`、远端路径和完整 build 输出默认不提交。
