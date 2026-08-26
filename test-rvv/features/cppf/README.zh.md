# CPPF RVV Topic

当前 EvidenceDecision（证据决策）是 `bench-only diagnostic / no-production`。Phase 000 的测试专用 RVV 候选能通过正确性对拍，但板卡 repeated benchmark（重复性能测试）显示 `pair/HSV` 和 `alpha_m` 两条候选均稳定慢于标量 reference，因此本 topic 不修改 production（生产源码），也不创建 `doc-rvv/features/cppf-RVV.zh.md`。

## 阅读入口

| 目的 | 路径 |
| --- | --- |
| 函数级评估、Traceability Map（可追踪性地图）和 no-production 决策 | `test-rvv/features/cppf/doc/cppf-evaluation.zh.md` |
| 跨 phase 候选搜索空间和恢复条件 | `test-rvv/features/cppf/doc/optimization-roadmap.zh.md` |
| phase 索引和默认恢复入口 | `test-rvv/features/cppf/doc/phases/README.zh.md` |
| optimization matrix（优化矩阵） | `test-rvv/features/cppf/doc/phases/optimization-matrix.zh.md` |
| Phase 000 结果和 doc-suite parity（文档套件对齐）审计 | `test-rvv/features/cppf/doc/phases/000-current-state-and-gaps/result.zh.md` |

## 常用命令

| 命令 | 证据角色 |
| --- | --- |
| `make -C test-rvv/features/cppf run_test_compare` | QEMU Std/RVV correctness（正确性）对拍。 |
| `make -C test-rvv/features/cppf dump_bench_rvv` | 生成 RVV bench 反汇编，用于确认候选 helper 有 RVV 指令归属。 |
| `make -C test-rvv/features/cppf run_board_test fetch_board_logs OUTPUT_DIR_BOARD=log/board/test REMOTE_BOARD_OUTPUT_DIR=<remote-output-dir>` | 板卡 correctness smoke（正确性冒烟）。 |
| `make -C test-rvv/features/cppf BENCH_ARGS='--side 24 --index-count 64 --repeat 6 --iterations 8 --warmup 2 --case-filter all' board_repeated evidence_doctor_repeated` | 板卡 5-run repeated benchmark 和 Evidence Doctor（证据体检）。 |

QEMU benchmark 只允许作为 log-shape smoke（日志形状冒烟），不能写成性能结论。

## 证据白名单与提交边界

当前文档引用的 evidence summary（证据摘要）位于：

| 路径 | 状态 | 用途 |
| --- | --- | --- |
| `test-rvv/features/cppf/log/board/repeated/evidence_manifest.json` | local-only by default；若用户要求提交 evidence logs，可用 `git add -f` 精确加入 | repeated board manifest（重复板卡证据清单）。 |
| `test-rvv/features/cppf/log/board/repeated/evidence_doctor.md` | local-only by default；若用户要求提交 evidence logs，可用 `git add -f` 精确加入 | Evidence Doctor 报告。 |
| `test-rvv/features/cppf/log/board/test/run_test.log` | local-only raw/smoke log | 板卡 gtest 5/5 通过记录。 |
| `test-rvv/features/cppf/build/asm/riscv/bench_cppf_rvv.full.asm` | build artifact，默认不提交 | 反汇编抽查输入。 |

默认提交边界是 topic 源码、测试、bench、script 和文档；`build/**` 与 `log/**` 仍按 `test-rvv/.gitignore` 排除，除非用户进入 evidence-log 提交阶段。

## 当前测试资产

| 文件 | 角色 |
| --- | --- |
| `include/cppf.h` | test-support aggregator（测试支撑聚合入口）。 |
| `include/impl/cppf_reference.hpp` | 标量 reference、fixture（测试夹具）和 checksum（校验和）。 |
| `include/impl/cppf_pair_hsv_candidate.hpp` | test-only SoA staging + RVV pair/HSV candidate。 |
| `include/impl/cppf_alpha_candidate.hpp` | test-only `alpha_m` closed-form audit 和 RVV candidate。 |
| `src/test_cppf.cpp` | correctness gtest。 |
| `src/bench_cppf.cpp` | benchmark wrapper（性能测试包装器）。 |
| `script/generate_cppf_evidence_manifest.py` | topic-local Evidence Doctor manifest 生成脚本。 |
