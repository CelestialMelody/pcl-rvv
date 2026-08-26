# Phase 030 Result: arbitrary-indices-gather-diagnostic

## 当前结论

本阶段完成了 indexed row source（索引行来源）在 topic-local production-shaped diagnostic（生产形态诊断）里的闭环：`segmentPolygonalPrismRvvFullScanCandidate` 对非 dense ordered indices 不再直接 fallback，而是在合法 `PointXYZ` / `float` / 32-bit byte offset 范围内使用 `vluxei32.v` 做原始点 `x/y/z` gather（离散加载），`projected_points` 仍按扫描顺序跨步加载，并用 `vcompress.vm` 保序输出源索引。

EvidenceDecision（证据决策）更新为 `diagnostic-positive`。`make run_board_eppd_repeated_indexed` 在板卡恢复后完成 5-run repeated board（重复板卡性能测试），`log/board/repeated-indexed/summary.md` 给出 median 2.20x、min 1.97x、max 2.32x，Evidence Doctor（证据体检）为 Errors=0 / Warnings=0 / Suggestions=0。该结果支持把 indexed gather 纳入后续 production probe（有界生产探针），但它仍是测试资产内的诊断证据，不等同于 production public（公开入口生产证据）。

## 执行范围回填

| action | 状态 | 命令 / 证据 | 结论 |
| --- | --- | --- | --- |
| RED：indexed full-scan test | done | `make run_test_rvv` 曾失败；失败点为 `path == ReferenceFallback` | 测试隔离了 indexed RVV 路径未实现，fixture 期望值与 reference 一致 |
| GREEN：实现 indexed gather helper | done | `include/impl/eppd_candidates.hpp` | 非 dense indices 使用 `vle32` 读取 source index，生成 32-bit byte offset 后对原始点字段 gather；dense 路径保持 stride load（跨步加载） |
| correctness（正确性） | done | `make run_test_compare` | Std 2 tests pass，RVV 11 tests pass |
| bench indexed mode | done | `src/bench_eppd.cpp`；QEMU smoke：`--size 4096 --iterations 1 --warmup 0 --indices indexed` | Std/RVV checksum 均为 `11874477441850024409`；QEMU 计时不作为性能证据 |
| manifest row source | done | `script/generate_eppd_board_evidence_manifest.py`、`make run_board_evidence_doctor` | indexed summary 解析为 `row_source: source_indexed_points`，Evidence Doctor 为 0 / 0 / 0 |
| asm attribution（反汇编归属） | done | `make dump_bench_rvv`；`build/asm/riscv/bench_eppd_rvv.full.asm` | 可见 `vluxseg3ei32.v`、`vlse32.v`、`vcompress.vm` 等指令；production path 中公共 wrapper 对紧密 xyz 选择 segmented gather |
| indexed board repeated | done | `make run_board_eppd_repeated_indexed` | `log/board/repeated-indexed/summary.md`：5 runs，median 2.20x，min 1.97x，max 2.32x |
| indexed board harness | done | `Makefile` | `run_board_eppd_repeated_indexed` 使用独立 `log/board/repeated-indexed` 本地目录，并把远端输出映射到独立 repeated-indexed 目录，避免覆盖 dense repeated 证据 |

## 证据分层

| 证据层 | 当前状态 | 能证明 | 不能证明 |
| --- | --- | --- | --- |
| correctness | pass | indexed RVV helper 在测试输入上保持 full-scan reference 输出和 source index 顺序 | 完整 production dispatch、所有点类型或全部 polygon 形态 |
| QEMU smoke | pass | bench 二进制在 indexed 模式可运行，Std/RVV checksum 一致 | 目标硬件性能 |
| asm | pass | RVV binary 中存在 indexed gather、dense stride load 和 compress 指令 | 单独证明运行时热度 |
| board performance | pass | indexed production-shaped diagnostic 在 Milkv-Jupiter 上是 positive bucket | 真实 `segment` 公开入口收益 |
| Evidence Doctor | pass | `log/board/repeated-indexed/evidence_doctor.md` 为 0 / 0 / 0 | reviewer 仍需复核证据角色和源码边界 |

## diagnostic-to-production mismatch audit 回填

| question | answer |
| --- | --- |
| evidence role | `production-shaped diagnostic`；仍是测试资产内 helper，不是 production public |
| A/B boundary | `test_support_full_scan`，Std/RVV 同一 bench wrapper，indexed 模式下两侧使用同一 indices 和 projected_points |
| 当前决策问题 | indexed row source 是否值得纳入后续 production probe |
| diagnostic 是否可外推到 production | partial：row source 更接近 production 的 `(*indices_)[i]` 访问，但本阶段仍不含真实 `segment` 分流和对象生命周期 |
| comparison-boundary / baseline mismatch 风险 | 本阶段 summary、manifest 和 doctor 已标注 `source_indexed_points`；production 接入仍需 public overload 边界重测 |
| weak / negative 时是否允许 bounded production probe | 本轮结果为 positive；若后续 production public 变弱或转负，应在 PI5 停下等待采纳或回滚确认 |
| clean adoption 是否需要 production boundary A/B | yes；本阶段只支持把 indexed 放入 PI2-PI5 候选范围 |

## 32-bit offset gate

测试资产中的 indexed helper 使用 `canUseUint32PointByteOffsets` gate（验收条件）：非 dense indices 必须非负、落在 `points.size()` 内，并且 `points.size() * sizeof(PointXYZ)` 可由 32-bit byte offset 表示；不满足时 fallback 到 reference。production 接入阶段改用 `pcl::rvv::rvvMaxU32ByteOffsetElements<PointT>()` 和真实 `indices_` 扫描来保护 public entry。

## Evidence Doctor 和 registry 状态

`log/board/repeated-indexed/evidence_manifest.json` 和 `log/board/repeated-indexed/evidence_doctor.md` 已生成。`log/evidence_registry.json` 已记录 dense / indexed diagnostic 和 production public 四组 summary、manifest、doctor；该 registry 位于 ignored `log/` 目录，默认不进入提交。

## continue / stop decision

本阶段完成。indexed gather 的诊断证据不再阻塞生产接入探针；Phase 040 已在真实 `ExtractPolygonalPrismData<PointT>::segment` public entry 中同时覆盖 dense 和 indexed 两条 row source。下一恢复入口是 `doc/phases/040-production-integration-loop/result.zh.md`，它停在 PI5 `pending_user_confirmation_adopt_production`。
