# DOTMOD Template Matching RVV Topic

## 本目录做什么

本 topic 针对 `recognition/src/dotmod.cpp` 中
`pcl::DOTMOD::detectTemplates()` 建立 RVV（RISC-V Vector，可变长度向量扩展）
函数级评估、生产直连测试和板卡证据。当前 production（生产源码）已在
`__RVV10__ && __riscv_vector` 构建下采用 direct-window RVV score（直接窗口 RVV 计分）
和 `responses` 缓冲复用；非 RVV 构建继续使用原 `getSubMap()` 标量路径。

## 阅读顺序

1. `doc/dotmod_template_matching-evaluation.zh.md`：函数级评估、候选取舍、Traceability Map（可追踪性地图）和最终 EvidenceDecision（证据决策）。
2. `doc/phases/README.zh.md`：阶段索引和恢复入口。
3. `doc/phases/030-response-buffer-reuse/result.zh.md`：当前生产证据结果。
4. `doc/phases/optimization-matrix.zh.md`：候选矩阵和未覆盖范围。
5. `doc/optimization-roadmap.zh.md`：当前 topic 内的继续 / 停止判断。
6. `doc-rvv/recognition/dotmod_template_matching-RVV.zh.md`：adopted production behavior（已采纳生产行为）的长期维护说明。

## 常用命令

- `make run_test_compare`：运行 diagnostic（诊断）Std/RVV correctness（正确性）对拍。
- `make dump_bench_rvv`：生成 diagnostic RVV bench（性能测试）反汇编。
- `make run_production_direct_test_compare`：运行真实 `DOTMOD::detectTemplates()` 生产直连 Std/RVV correctness 对拍。
- `make dump_production_direct_bench_rvv`：确认生产直连 bench binary 中 `dotmodScoreWindowDirectRVV` 和 `vle8` / `vand` / `vmsne` / `vcpop` 归属。
- `SSH_AUTH_SOCK=/run/user/1001/keyring/ssh make collect_production_direct_repeated_board BENCH_ARGS='256 192 24 16 100 5 8 2 0.9 1'`：板卡 5-run production-public（真实公开入口）性能采集。
- `make record_production_direct_evidence_state BENCH_ARGS='256 192 24 16 100 5 8 2 0.9 1'`：生成 summary、manifest、Evidence Doctor（证据体检）并刷新 registry（证据登记表）。
- `make check_production_direct_evidence_freshness`：检查当前 production direct summary / manifest / doctor 是否已登记并被文档引用。

## 当前状态

当前 production direct 板卡证据位于
`log/board/repeated_phase020_production_direct/summary.md`。目录名沿用 Phase 020 target，
内容已经由 Phase 030 的 response-buffer-reuse 复跑覆盖；长期文档和 evaluation 以该 summary
作为当前证据。

当前结果为 `dotmod_production_detecttemplates_total` 5-run median speedup `3.298x`，
range `3.276x - 3.304x`，`B/A < 1 = 0/5`，checksum 一致。Evidence Doctor 结果为
`Errors=0`、`Warnings=0`、`Suggestions=2`。两个 suggestion 分别是环境 metadata（元数据）
和 binary identity（二进制身份）缺失，不阻塞当前 positive decision bucket（正向决策桶）。

## 证据白名单和提交边界

默认 evidence policy（证据策略）是 `summary-only`。可提交候选是 summary、manifest、
Evidence Doctor 和 registry 级摘要；raw board logs（原始板卡日志）、`build/` 二进制、
QEMU 运行日志和 `__pycache__/` 不属于默认提交边界。

本 topic 当前涉及的生产源码是 `recognition/src/dotmod.cpp`。其它 recognition topic 的未跟踪
`doc-rvv/recognition/*-RVV.zh.md` 或无关 dirty 文件不属于本 topic 的审查边界。
