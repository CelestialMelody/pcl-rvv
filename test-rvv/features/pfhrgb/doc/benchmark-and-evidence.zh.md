# PFHRGB Benchmark And Evidence

本文负责 bench（性能测试）case、summary-only（仅摘要）证据、Evidence Doctor（证据体检）和提交边界。
correctness TEST 细节见 `doc/correctness-tests.zh.md`。

## Bench 输出格式

`src/bench_pfhrgb.cpp` 每个 case 输出 `<label>: <ms> ms / iter` 和 `<label> checksum: <value>`。
`--case-filter <label>` 可只运行一个 case。默认 dataset line 是
`synthetic pfhrgb rgb-normal grid side=32 points=1024 k=32`，板卡 repeated 使用 iterations 8、warmup 2。

## Case-filter 字典

| case | 计时边界 | 证据角色 | 当前 board decision |
| --- | --- | --- | --- |
| `component_pfhrgb_signature` | scalar reference helper repeated 128 次。 | production-shaped diagnostic 背景。 | neutral-negative；Doctor Warning。 |
| `candidate_pfhrgb_pair_batch_rvv` | pair-batch RVV helper repeated 128 次，含 SoA staging 和 scalar histogram scatter。 | diagnostic component。 | negative-current-rerun；Doctor Error。 |
| `public_pfhrgb_k` | `PFHRGBEstimation::compute` + KdTree KSearch，命中当前 exact-gated production RVV dispatch。 | production-public（生产公开入口）。 | positive；median `1.27x`，支撑当前采纳。 |
| `public_pfhrgb_k_with_candidate` | topic-local KSearch 外层循环，每个点调用非复用 candidate helper。 | production-shaped diagnostic。 | positive，median `1.21x`。 |
| `public_pfhrgb_k_with_candidate_reuse` | topic-local KSearch 外层循环，跨点复用 `PairBatchWorkspace`。 | allocation / staging ablation。 | positive，median `1.24x`。 |

## 当前 board 证据

当前 summary path 是 `log/board/repeated/evidence_manifest.json`，Doctor path 是
`log/board/repeated/evidence_doctor.md`。本轮 5-run 结果：

| case | Std mean | RVV mean | B/A values | bucket |
| --- | --- | --- | --- | --- |
| `public_pfhrgb_k` | `583.0768 ms` | `458.2580 ms` | `1.28, 1.27, 1.28, 1.27, 1.27` | positive production-public |
| `public_pfhrgb_k_with_candidate_reuse` | `565.2896 ms` | `456.2476 ms` | `1.23, 1.23, 1.25, 1.25, 1.24` | positive diagnostic |
| `public_pfhrgb_k_with_candidate` | `567.5496 ms` | `467.3170 ms` | `1.21, 1.21, 1.22, 1.22, 1.21` | positive diagnostic |
| `candidate_pfhrgb_pair_batch_rvv` | `68.4026 ms` | `69.8494 ms` | `0.98, 0.97, 0.99, 0.98, 0.97` | negative-current-rerun |
| `component_pfhrgb_signature` | `68.5921 ms` | `68.2119 ms` | `1.00, 1.00, 1.02, 1.01, 0.99` | neutral-negative |

## Evidence Doctor 回填

当前 Doctor 结果是 Errors=1 / Warnings=1 / Suggestions=6。

| severity | case | 处理动作 | 结论影响 |
| --- | --- | --- | --- |
| Error | `candidate_pfhrgb_pair_batch_rvv` | 降级 helper-only 收益，不再把历史 positive 当当前 truth。 | 不阻塞 public-shaped reuse positive；阻塞 helper-only production value 外推。 |
| Warning | `component_pfhrgb_signature` | 保持 neutral-negative。 | 不能作为收益证据。 |
| Suggestion | all cases | 缺少 taskset、governor、freq、temperature。 | Handoff 记录 metadata 边界；PI1 可继续，但生产证据仍需解释。 |
| Suggestion | `component_pfhrgb_signature` | near-threshold。 | 保持 neutral-negative，不写成 speedup。 |

Manifest 记录 binary hash：

- `bench_std=sha256:b268f1de17e8c979ad8fcf429c7798ac4b34d08da7d49eb16d9954650a3fd0d9`
- `bench_rvv=sha256:51296f3b454eca52a988af90bdf3d8fc4b30bfcbfadb890e4ec38ce04bef1f38`

## ASM Attribution 口径

`make -B -C test-rvv/features/pfhrgb check_production_rvv_symbol` 会先生成
`build/asm/riscv/bench_pfhrgb_rvv.full.asm`，再检查 `computePointPFHRGBSignatureRVV` 符号。
当前该 gate 已通过，`vsetvli`、`vle32.v`、`vfmacc.vv`、`vfdiv.vv` 等 RVV 指令可归入接入后的
production RVV helper；它仍不证明非 exact 点型会命中 RVV。

## 复现命令和提交边界

| 命令 | 作用 |
| --- | --- |
| `make -C test-rvv/features/pfhrgb run_bench_rvv BENCH_ARGS="--side 8 --k 8 --iterations 1 --warmup 0 --case-filter public_pfhrgb_k_with_candidate_reuse"` | QEMU log-shape smoke，不做性能结论。 |
| `make -C test-rvv/features/pfhrgb REPEATED_BOARD_RUNS=5 board_repeated evidence_doctor_repeated` | 板卡 5-run repeated 和 Doctor。 |
| `make -C test-rvv/features/pfhrgb evidence_doctor_repeated` | 从已拉回 summary 重建 manifest / Doctor。 |

`log/board/repeated/evidence_manifest.json`、`evidence_doctor.md`、`evidence_doctor.json` 和
`log/evidence_registry.json` 是当前可引用摘要。`log/board/repeated/run_*` 和 `log/qemu/*` 是 raw log，
默认 local-only。
