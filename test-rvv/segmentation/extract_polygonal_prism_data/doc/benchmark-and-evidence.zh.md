# Benchmark 与证据

本文说明 `bench_eppd` 的 CLI、case label（用例标签）、板卡 summary 和 Evidence Doctor 提交边界。性能结论只来自 Milkv-Jupiter 板卡 repeated summary；QEMU 只用于 correctness / log-shape（正确性和日志形状）。

## CLI 与 case label

`src/bench_eppd.cpp` 支持这些关键参数：

| 参数 | 取值 | 含义 |
| --- | --- | --- |
| `--path` | `diagnostic` / `production` | diagnostic 调测试专用 candidate；production 调真实 `ExtractPolygonalPrismData::segment` |
| `--indices` | `dense` / `indexed` | dense 使用有序 indices；indexed 使用非连续 source index，触发 gather |
| `--polygons` | `single` / `nested` | single 使用单 polygon；nested 使用多 polygon XOR |
| `--point-type` | `xyz` / `xyzi` / `xyzrgb` / `xyzrgba` / `xyzinormal` | production path 的点型选择；非 `xyz` 只用于 production |
| `--size` | 正整数 | 输入点数，也是 production RVV scale gate 的主要 work item count |
| `--iterations` / `--warmup` | 正整数 | 计时迭代与 warm-up（预热）次数 |

case label 会把 path、indices、polygons、point-type 和 size 写进输出，summary script 用这些字段生成 manifest。

## Repeated Board 证据

| 证据组 | 路径 | 当前结论 |
| --- | --- | --- |
| diagnostic dense | `log/board/repeated/summary.md` | median 1.98x；用于升级 production probe，不是最终生产证据 |
| diagnostic indexed | `log/board/repeated-indexed/summary.md` | median 2.20x；用于 indexed production probe |
| production dense | `log/board/repeated-production/summary.md` | median 1.75x，doctor 0 / 0 / 0 |
| production indexed | `log/board/repeated-production-indexed/summary.md` | median 1.75x，doctor 0 / 0 / 0 |
| production nested dense | `log/board/repeated-production-nested/summary.md` | median 2.18x，doctor 0 / 0 / 0 |
| production nested indexed | `log/board/repeated-production-nested-indexed/summary.md` | median 2.13x，doctor 0 / 0 / 0 |
| production point types | `log/board/repeated-production-*-postgate/summary.md` | `PointXYZI` 1.85x、`PointXYZRGB` 1.83x、`PointXYZRGBA` 1.84x；`PointXYZINormal` fallback 1.00x |
| threshold 32 confirm5 | `log/board/repeated-production-size32-threshold32-confirm5/summary.md` | median 1.19x、min 1.18x、doctor 0 / 0 / 0，支撑阈值降到 32 |

## Evidence Doctor 和 registry

`script/generate_eppd_board_evidence_manifest.py` 把 repeated run 输出转成 `evidence_manifest.json`，`make run_board_evidence_doctor` 调用共享 `test-rvv/script/evidence_doctor.py` 生成 `evidence_doctor.md`。`log/evidence_registry.json` 是本地 freshness（新鲜度）登记表，默认不提交；提交前用 `evidence_registry.py check --require-doc-ref --fail-on any` 确认 summary / manifest / doctor 与文档引用一致。

## 提交边界

summary-only（只提交摘要证据）策略下，允许提交被 README、evaluation、phase result 或正式 `doc-rvv` 引用的 `summary.md`、`evidence_manifest.json` 和 `evidence_doctor.md`。raw run logs、QEMU logs、build asm 和本机路径不提交。
