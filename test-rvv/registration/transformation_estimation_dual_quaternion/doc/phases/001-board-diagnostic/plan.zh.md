# Phase 001 Plan：board-diagnostic

## 目标

在不修改 production 源码的前提下，补齐 `ordered-cloud-pair` 的板端 repeated diagnostic（重复诊断）证据。该阶段只回答 test-only ordered-cloud-pair C1/C2 RVV accumulation candidate 是否值得进入 PI1；不直接采纳 production dispatch。

## 行动表

| action | 内容 | 输出 | 早停条件 |
| --- | --- | --- | --- |
| A1 board repeated 支撑 | 新增 topic-local board repeated collect / summary / doctor / registry target | `Makefile`、`script/generate_tedq_board_repeated_summary.py` | Make target 无法复用共享 board workflow |
| A2 板端采集 | 在 `Milkv-Jupiter` 上运行 5-run repeated，`--case-filter ordered-cloud-pair` | `log/board/rvv_accum_full_cloud_repeated/run-*` raw logs（ignored-local） | 板卡不可达或 checksum mismatch |
| A3 证据摘要 | 生成 `summary.md`、`evidence_manifest.json`、`evidence_doctor.md` | summary-only evidence | Evidence Doctor Error |
| A4 registry / 文档 | 刷新 `log/evidence_registry.json` 和 topic 文档 | README、evidence、roadmap、phase result | doc-ref 缺失或 evidence_status 异常 |

## 运行合同

- case-filter：`ordered-cloud-pair`
- repeated runs：`5`
- iterations：`20`
- warm-up iterations：`5`
- B/A：`Std helper ms / RVV helper ms`
- 证据边界：同一 test-support helper 的 Std/RVV 构建对比；不是 production direct。

## 通过条件

1. Std/RVV checksum 每个规模一致。
2. 64K / 256K 的 repeated B/A bucket 不为 `negative`。
3. Evidence Doctor：Errors=0。
4. production 文件 `registration/include/pcl/registration/impl/transformation_estimation_dual_quaternion.hpp` 保持未修改。
