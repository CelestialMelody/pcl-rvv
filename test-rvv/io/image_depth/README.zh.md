# image_depth RVV topic navigation

## 当前结论

`io/src/image_depth.cpp` 的 `DepthImage::fillDepthImage()` / `fillDisparityImage()` 已完成 PI5 production decision checkpoint（生产决策检查点）并由用户确认采纳有收益的 production patch。当前 adopted production behavior（已采用生产行为）是 depth contiguous（深度连续）、disparity contiguous（视差连续）和 disparity downsample（视差下采样）RVV helper；depth downsample（深度下采样）在接入后证据中不值得保留 RVV，当前保持标量 fallback。

正式长期文档已创建：`doc-rvv/io/image_depth-RVV.zh.md`。长期文档只记录当前生产行为、dispatch / fallback、production-public（真实公开入口）板卡数据和维护边界；早期 diagnostic（诊断）过程仍归属到本 topic-local 文档套件。

## 阅读顺序

| 目的 | 先读 |
| --- | --- |
| 恢复当前阶段 | `doc/phases/README.zh.md` |
| 理解函数级决策 | `doc/image_depth-evaluation.zh.md` |
| 查看生产接入边界 | `doc/phases/010-production-integration-plan/plan.zh.md` |
| 查看生产接入结果 | `doc/phases/050-production-public-probe/result.zh.md` |
| 查看测试入口和覆盖 | `doc/testing-overview.zh.md`、`doc/correctness-tests.zh.md` |
| 查看 bench / board / Evidence Doctor | `doc/benchmark-and-evidence.zh.md` |
| 查看候选取舍 | `doc/optimization-evidence.zh.md`、`doc/optimization-roadmap.zh.md` |
| 定位测试支撑代码 | `doc/test-support-code-map.zh.md` |

## 常用命令

| 命令 | 作用 | 证据边界 |
| --- | --- | --- |
| `make run_test_compare` | Std/RVV 两个 build 的 correctness（正确性）对拍 | 同时覆盖 helper-level diagnostic 和 production-public path hit。 |
| `make run_bench_rvv BENCH_ARGS="--case-filter all --iterations 2 --warmup-iterations 1"` | QEMU smoke（仿真小型验证） | 只检查可运行和日志形状，不作性能结论。 |
| `make dump_bench_rvv` | 生成 RVV bench 反汇编 | 可归属到 production helper / public entry 内联边界。 |
| `make record_board_repeated_evidence_state` | 重建 contiguous repeated summary / manifest / doctor 并登记 registry | 板卡 production-shaped diagnostic。 |
| `make record_board_downsample_evidence_state` | 重建 downsample repeated summary / manifest / doctor 并登记 registry | 板卡 production-shaped diagnostic。 |
| `make record_board_production_evidence_state` | 重建 production-public repeated summary / manifest / doctor 并登记 registry | 板卡 production-public。 |
| `make evidence_status` | 检查 summary / manifest / doctor 与 registry 和文档引用是否 fresh | freshness guard（新鲜度保护）。 |

## 当前可提交证据

summary-only（只提交摘要）候选：

- `log/evidence_registry.json`
- `log/board/repeated_contiguous/summary.md`
- `log/board/repeated_contiguous/evidence_manifest.json`
- `log/board/repeated_contiguous/evidence_doctor.md`
- `log/board/repeated_downsample/summary.md`
- `log/board/repeated_downsample/evidence_manifest.json`
- `log/board/repeated_downsample/evidence_doctor.md`
- `log/board/repeated_production_public/summary.md`
- `log/board/repeated_production_public/evidence_manifest.json`
- `log/board/repeated_production_public/evidence_doctor.md`

默认不提交：

- `build/`
- `log/qemu/*.log`
- `log/board/*.log`
- `log/board/**/run*.log`
- `log/board/**/analyze_bench_compare_*.log`
- 本机 `config.mk`、私有板卡地址和远端路径。

## production topic doc

`doc-rvv/io/image_depth-RVV.zh.md` 已创建，并使用 `log/board/repeated_production_public/summary.md` 的生产接入后板卡数据。该长期文档不承载 raw log 或早期 phase 流水。
