# harris_2d Benchmark And Evidence

## bench case 字典

| case label | 输入 | method / window | 计时边界 | 证明点 |
| --- | --- | --- | --- | --- |
| `harris2d_harris_320x240` | 320x240 synthetic organized image | Harris / 3x3 configured window | public `compute()` with `nonmax=false` | 常规 response map 公开入口吞吐 |
| `harris2d_tomasi_320x240` | 320x240 synthetic organized image | Tomasi / 3x3 configured window | public `compute()` with `nonmax=false` | 包含 `sqrt` 的 response formula |
| `harris2d_noble_tail_641x481` | 641x481 synthetic organized image | Noble / 5x5 configured window | public `compute()` with `nonmax=false` | RVV tail lanes、更大窗口和多尺寸状态回归 |

## 证据入口

| target | 输出 | 证据角色 |
| --- | --- | --- |
| `run_test_compare` | `log/qemu/run_test_std.log`、`log/qemu/run_test_rvv.log` | QEMU correctness（正确性） |
| `run_bench_* BENCH_ARGS="--public-entry"` | QEMU 小型可运行性 smoke | 日志形状和 correctness 行检查，不是性能证据 |
| `check_harris_2d_rvv_asm` | `build/asm/riscv/bench_harris_2d_rvv.asm` | asm attribution（反汇编归属） |
| `board_repeated` | `log/board/repeated_phase020_direct_intensity_stride_store_public_entry/run_*/` | raw board logs，默认不提交 |
| `record_evidence_state_repeated` | `summary.md`、`evidence_manifest.json`、`evidence_doctor.md`、`evidence_registry.json` | 可提交摘要证据和 freshness（新鲜度）检查 |

## 当前 production direct 结果

| case | runs | median speedup | min | max | B/A < 1 | correctness |
| --- | ---: | ---: | ---: | ---: | ---: | --- |
| `harris2d_harris_320x240` | 5 | 1.070x | 1.050x | 1.080x | 0/5 | tolerance pass |
| `harris2d_tomasi_320x240` | 5 | 1.090x | 1.080x | 1.110x | 0/5 | tolerance pass |
| `harris2d_noble_tail_641x481` | 5 | 1.180x | 1.180x | 1.190x | 0/5 | tolerance pass |

证据路径：`test-rvv/keypoints/harris_2d/log/board/repeated_phase020_direct_intensity_stride_store_public_entry/summary.md`。该 summary 的 `evidence_role` 是 `production_direct`，A/B boundary（A/B 边界）是 `public overload`。

## checksum 和 correctness policy

bench raw log 继续输出 `checksumResponses()`，manifest 中将其保留为 `raw_response_checksum`。Evidence Doctor 使用的 `baseline.checksum` / `candidate.checksum` 是 tolerance-based semantic fingerprint（基于误差阈值的语义指纹）；它只在各 build 输出相对 scalar reference 通过当前 tolerance 时相等。

public-entry bench 使用 `1e-3` tolerance，因为 320x240 Harris public 输出相对独立 reference 的最大误差稳定约为 `5.34e-4`。diagnostic candidate 对拍仍使用 `1e-4`。

## Evidence Doctor

当前 `evidence_doctor.md` 结果为 Errors=0、Warnings=0、Suggestions=6。Suggestions 只指出环境 metadata 和 binary identity 缺失；本轮没有方向反转或长尾退化，因此不阻塞 weak-positive production adoption。若后续需要发布级性能报告，应补记录 taskset、governor、freq、temperature 和 binary hash。

## 提交边界

性能结论只引用 repeated board summary、manifest 和 Evidence Doctor。raw logs 和 build output 默认不提交；若用户要求提交日志，先运行 topic 或全局脱敏检查，并把日志 commit 与 production / 文档 commit 拆分。
