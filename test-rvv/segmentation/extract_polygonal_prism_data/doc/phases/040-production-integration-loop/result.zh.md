# Phase 040 Result: production-integration-loop

## 当前结论

本阶段完成 PI2-PI5 production integration loop（生产接入闭环）。生产补丁已把 `ExtractPolygonalPrismData<PointT>::segment(PointIndices&)` 拆成 public dispatch（公开入口分流）、`segmentStd` 标量 fallback（回退路径）和 `segmentRvv` RVV helper（RVV 辅助实现）。`__RVV10__` 构建下先尝试 RVV，任一 gate 不满足时回到 `segmentStd`；非 RVV 构建只保留标量路径。

PI5 EvidenceDecision（生产证据决策）在本阶段完成时是 `pending_user_confirmation_adopt_production`。production public（公开入口生产证据）在 Milkv-Jupiter 板卡上为 positive bucket：dense median 1.75x，indexed median 1.75x；两组 Evidence Doctor 都是 Errors=0 / Warnings=0 / Suggestions=0。按照 AGENTS.md 的 PI5 对称检查点规则，本阶段当时必须保留 patch 并等待用户明确确认采纳。

Phase 045 已根据用户确认把该补丁记录为 adopted production behavior（已采用生产行为），并创建正式 `doc-rvv/segmentation/extract_polygonal_prism_data-RVV.zh.md`。因此本文件保留 PI5 停止点事实，当前恢复入口以 `doc/phases/045-production-closeout-after-adoption/result.zh.md` 为准。

## 计划动作回填

| action | 状态 | 命令 / 证据 | 结论 |
| --- | --- | --- | --- |
| TDD RED：production direct helper 测试 | done | RVV 构建下测试先要求 `segmentStd` / `segmentRvv` helper | 生产类需要清晰 Std/RVV 分层，RED 能暴露 helper 缺失 |
| PI2 clean split | done | `segmentation/include/pcl/segmentation/extract_polygonal_prism_data.h`、`segmentation/include/pcl/segmentation/impl/extract_polygonal_prism_data.hpp` | public `segment` 只做 RVV 尝试和 `segmentStd` fallback；原标量主体移入 `segmentStd` |
| RVV helper | done | `segmentRvv` | 覆盖 `RVVXYZAoSFloatLayout<PointT>`、`indices_->size() >= 64`、合法 indices、32-bit byte offset、single polygon |
| production direct correctness | done | `make run_test_compare`、`make run_board_test` | Std 2 tests pass，RVV 11 tests pass；板卡 correctness 11 tests pass |
| production QEMU smoke | done | `make run_bench_std BENCH_ARGS="--size 4096 --iterations 1 --warmup 0 --path production"`；对应 RVV 命令；indexed 同形态 smoke | dense 和 indexed 的 Std/RVV checksum 一致；QEMU 计时不作为性能证据 |
| asm attribution | done | `make dump_bench_rvv`；`build/asm/riscv/bench_eppd_rvv.full.asm` | production `segmentRvv` 符号内可见 `vlse32.v`、`vluxseg3ei32.v`、`vcompress.vm`、`vfmacc.vf`、`vmxor.mm` |
| board production dense | done | `make run_board_eppd_repeated_production` | `log/board/repeated-production/summary.md`：5 runs，median 1.75x，min 1.74x，max 1.75x |
| board production indexed | done | `make run_board_eppd_repeated_production_indexed` | `log/board/repeated-production-indexed/summary.md`：5 runs，median 1.75x，min 1.72x，max 1.75x |
| Evidence Doctor | done | `log/board/repeated-production/evidence_doctor.md`、`log/board/repeated-production-indexed/evidence_doctor.md` | 两组 production public 证据均为 0 / 0 / 0 |

## 生产补丁范围

| 文件 | 变更 | 边界 |
| --- | --- | --- |
| `segmentation/include/pcl/segmentation/extract_polygonal_prism_data.h` | 新增 protected `segmentStd(PointIndices&)` 和 `segmentRvv(PointIndices&)` 声明，后者仅在 `__RVV10__` 下存在 | 不改 public API |
| `segmentation/include/pcl/segmentation/impl/extract_polygonal_prism_data.hpp` | public `segment` 变成短路分流；新增 `segmentRvv`；原标量主体移入 `segmentStd` | 不改 `isPointIn2DPolygon` / `isXYPointIn2DXYPolygon` |
| `test-rvv/segmentation/extract_polygonal_prism_data/src/test_eppd.cpp` | 新增 production direct 和 fallback tests | 测试专用派生类访问 protected helper |
| `test-rvv/segmentation/extract_polygonal_prism_data/src/bench_eppd.cpp` | 新增 `--path diagnostic|production`，production 模式调用真实 public `segment` | QEMU smoke 只验证 checksum；性能只看板卡 |
| `test-rvv/segmentation/extract_polygonal_prism_data/Makefile` | 新增 production repeated board targets，修复 indexed repeated 输出目录传参 | evidence 输出在 ignored `log/` 下，summary 可由文档引用 |
| `test-rvv/segmentation/extract_polygonal_prism_data/script/generate_eppd_board_evidence_manifest.py` | 识别 production public case、row source 和 timer boundary | manifest 供 Evidence Doctor 使用 |

## fallback 矩阵

| gate | production 行为 | 证据 |
| --- | --- | --- |
| 非 `__RVV10__` 构建 | public `segment` 直接调用 `segmentStd` | `make run_test_compare` 中 Std build 通过 |
| `input_` 或 `planar_hull_` 为空 | `segmentRvv` 返回 false，public 入口继续标量语义 | production helper 边界由源码保护 |
| `initCompute()` 失败 | `segmentRvv` 清空输出并返回 true，与标量入口一致 | helper 复刻原入口行为 |
| `RVVXYZAoSFloatLayout<PointT>` 不满足 | 编译期返回 false，fallback 到 `segmentStd` | `PointXYZI` traits-gated 正向；非 xyz AoS 类型保留标量 |
| work item count 小于 64 | fallback 到 `segmentStd` | `SegmentRvvDeclinesSmallInputs` |
| indices 非法或 32-bit byte offset 超界 | fallback 到 `segmentStd` | `segmentRvv` 逐项检查 index 范围，cloud size 检查 byte offset |
| `polygons_.size() > 1` 或 active polygon 顶点不足 | fallback 到 `segmentStd` | `SegmentRvvDeclinesConcaveHullPolygons` |
| projected_points 与 indices 数量不一致 | fallback 到 `segmentStd` | `segmentRvv` 检查投影输出尺寸 |

## production public 证据

| row source | 命令 | summary | result | doctor |
| --- | --- | --- | --- | --- |
| dense ordered indices | `make run_board_eppd_repeated_production` | `log/board/repeated-production/summary.md` | 5 runs，median 1.75x，min 1.74x，max 1.75x | `log/board/repeated-production/evidence_doctor.md`：0 / 0 / 0 |
| source indexed points | `make run_board_eppd_repeated_production_indexed` | `log/board/repeated-production-indexed/summary.md` | 5 runs，median 1.75x，min 1.72x，max 1.75x | `log/board/repeated-production-indexed/evidence_doctor.md`：0 / 0 / 0 |

这两组数据只证明当前 public RVV path 快于当前 public scalar path。当前 topic 没有已采用的旧 RVV family（实现族）可做 RVV-vs-RVV 选择，因此不需要同一 production boundary 内的 RVV-vs-RVV detail A/B 来比较新旧 RVV family。PI5 仍要求用户确认是否采纳。

## diagnostic-to-production mismatch audit 回填

| question | answer |
| --- | --- |
| evidence role | 本阶段为 `production-public`；Phase 010 / 030 为 `production-shaped diagnostic` |
| A/B boundary | public overload：Std build 和 RVV build 都通过真实 `ExtractPolygonalPrismData<PointT>::segment` |
| 当前决策问题 | 当前 public RVV path 是否比当前 public scalar path 快，且 fallback 是否保持语义 |
| diagnostic 是否可外推到 production | Phase 010 / 030 的诊断结果已由 production public 证据重新验证；最终生产判断以本阶段 production public summary 为准 |
| comparison-boundary / baseline mismatch 风险 | production dense 和 indexed manifest 标记了 `production_public`、public overload 和 public segment 计时边界；QEMU smoke 未用于性能结论 |
| weak / negative 时是否允许 bounded production probe | 本阶段结果为 positive；若 reviewer 复核发现证据边界不成立，应回到 PI5 等待用户决定是否补证或回滚 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | no；当前决策不是 RVV-family-selection，且没有已采用 RVV family 需要替换 |

## doc suite 和长期文档状态

| area | 状态 | 说明 |
| --- | --- | --- |
| topic navigation | adopted | `README.zh.md` 已刷新为 PI5 待确认入口 |
| evaluation | adopted | `doc/extract_polygonal_prism_data-evaluation.zh.md` 已记录生产补丁范围、fallback 矩阵和 production public 证据 |
| phase index / matrix / roadmap | adopted | `doc/phases/README.zh.md`、`doc/phases/optimization-matrix.zh.md`、`doc/optimization-roadmap.zh.md` 指向 Phase 040 |
| production topic doc | turn_stop_deferred with stop_condition_hit | PI5 证据支持采纳，但缺用户确认；正式 `doc-rvv/segmentation/extract_polygonal_prism_data-RVV.zh.md` 暂不创建 |
| artifact tracking | partial | topic 资产和 evidence summary 仍多为 untracked / ignored-local；提交前需由 reviewer 决定 staging 边界 |

## Evidence Doctor 和 registry 状态

`log/evidence_registry.json` 已登记四组 evidence summary（证据摘要）、manifest 和 doctor：dense diagnostic、indexed diagnostic、dense production public、indexed production public。registry 位于 ignored `log/` 下，默认不提交；提交前如需要保留 summary evidence，应使用 `git add -f` 精确加入 summary / manifest / doctor，raw run logs 默认不提交。

## continue / stop decision

本阶段完成时命中 PI5 停止条件 `production_adoption_requires_user_authorization`。该停止条件已由 Phase 045 解除：当前默认下一步是按 roadmap 判断是否进入 `050-concave-hull-xor`，或因后续方向收益 / 范围不值得而暂停。
