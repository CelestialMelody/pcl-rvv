# PPF Benchmark And Evidence

本文记录 `bench_ppf` 的 case label、计时边界、summary evidence（摘要证据）和提交边界。
性能结论只引用 Milkv-Jupiter board repeated benchmark（板卡重复性能测试）。

## Bench CLI

`src/bench_ppf.cpp` 支持：

- `--side <n>`：构造 `n * n` synthetic ppf grid。
- `--index-count <n>`：选择前 `n` 个 reference indices。
- `--repeat <n>`：每次计时内部重复次数。
- `--iterations <n>` / `--warmup <n>`：计时迭代和预热次数。
- `--case-filter <label>`：只运行某一个 bench case。

Phase 060 使用的 production-public 参数是：

```bash
--side 28 --index-count 64 --repeat 8 --iterations 8 --warmup 2
```

## Case 字典

| case label | 证据角色 | 计时边界 | 当前结论 |
| --- | --- | --- | --- |
| `component_ppf_reference` | diagnostic baseline（诊断基线） | test-only scalar reference | 只作组件比较基线。 |
| `candidate_ppf_pair_feature_batch_rvv` | component ablation（组件消融） | SoA staging + RVV `f1..f4` + scalar `alpha_m` + output write | Phase 010 board repeated negative，拒绝接入 production。 |
| `candidate_ppf_alpha_m_batch_rvv` | component ablation | scalar `f1..f4` + alpha staging + RVV `atan2` + output write | Phase 030 diagnostic positive，已被 Phase 040/060 production evidence 取代。 |
| `public_ppf_compute` | production-public | exact `PointXYZ + Normal -> PPFSignature` public `compute()` | Phase 040 adopted baseline，5-run speedup 约 `1.35x-1.39x`。 |
| `public_ppf_compute_pointxyzi_normal` | production-public | `PointXYZI + Normal -> PPFSignature` public `compute()` | Phase 060 adopted，mean Std `429.5662 ms`，mean RVV `310.3166 ms`。 |
| `public_ppf_compute_pointxyz_pointnormal` | production-public | `PointXYZ + PointNormal -> PPFSignature` public `compute()` | Phase 060 adopted，mean Std `412.4258 ms`，mean RVV `310.2756 ms`。 |

## 当前板卡证据

| evidence path | role | Doctor result | 结论 |
| --- | --- | --- | --- |
| `log/board/repeated/evidence_manifest.json` | Phase 040 exact public case summary | `0E/0W/2S` | exact gate production-public positive。 |
| `log/board/phase060-pointxyzi-normal/repeated/evidence_manifest.json` | Phase 060 source expansion summary | `0E/0W/2S` | `PointXYZI + Normal` public case positive。 |
| `log/board/phase060-pointxyz-pointnormal/repeated/evidence_manifest.json` | Phase 060 normal expansion summary | `0E/0W/2S` | `PointXYZ + PointNormal` public case positive。 |

Phase 060 两个 Evidence Doctor suggestion 都是 `environment_metadata_missing` 和
`binary_identity_missing`。它们不阻塞当前 positive bucket（正向决策桶），但严格归档时应补
taskset、governor、freq、temperature 和 binary hash。

## Evidence registry 状态

当前没有 `test-rvv/features/ppf/log/evidence_registry.json`。本 topic 采用人工 freshness check：
长期结论只引用 manifest / Doctor 摘要路径，raw `run_*/run_bench_*.log` 默认 local-only（仅本机保留）。
若未来提交 evidence logs（证据日志），应先运行脱敏检查，并可另开 evidence hardening phase 补
registry 和 binary identity。

## 提交边界

`doc/`、`include/`、`src/`、`Makefile` 和 `board.mk` 属于 topic test asset（主题测试资产）。
`build/`、raw `log/qemu/`、raw `log/board/**/run_*` 默认不提交。被文档引用的
`evidence_manifest.json` 和 `evidence_doctor.md/json` 可作为 summary-only evidence 进入审查候选。
