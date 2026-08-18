# Phase 006 Result：test-support split 与 row-source family comparison

## 阶段结论

Phase 006 已完成，结论为 `diagnostic_positive_no_production`。

- `tedq_candidates.hpp` 从 858 行降至 661 行；fixtures、row-source adapter、范围检查和
  materialize 辅助迁移到 `include/impl/tedq_adapters.hpp`。
- 新增 source-indexed direct indexed gather candidate；RVV 路径使用三次 `vluxei32` 读取
  source x/y/z，保持既有 `f32mf2 -> f64m1` widen/reduction 与 Eigen 4x4 solve。
- Std/RVV correctness 重新清理构建并通过，各 15 tests passed；新增
  `SourceIndexedDirectGatherCandidateMatchesScalar` 通过。
- board smoke 与 bounded 5-run repeated 均成功，5 次 run 的 case、checksum 和 manifest
  合同完整。
- source-indexed staged ordered reuse 与 direct indexed gather 在同一 RVV binary、同一
  row pairing、同一公式、同一 solve 和同一 checksum 边界下比较，4K / 64K / 256K 的
  median `B/A = staged_rvv_ms / direct_gather_rvv_ms` 为 `1.242x / 1.458x / 1.470x`，
  三组均为 `positive`。
- Evidence Doctor：`Errors=0，Warnings=0，Suggestions=0`。

该结果只批准 `test-rvv` 内的 source-indexed implementation-family diagnostic。它不能升级为
production direct、不能外推到 dual-indexed / correspondence，也不能推翻 Phase 002
production-public neutral 结论。

## 实施内容

| 项目 | 结果 |
| --- | --- |
| test-support split | 新增 `include/impl/tedq_adapters.hpp`；`include/tedq.h` 保持稳定聚合入口。 |
| direct gather candidate | 新增 `estimateDualQuaternionSourceIndexedDirectCandidate`；非 RVV 构建回退到标量。 |
| correctness | Std/RVV 各 15 tests passed；非 identity source indices 与 scalar reference 对拍通过。 |
| family bench | 新增 `row-source-family-comparison` case-filter，只输出 source-indexed staged/direct 两族。 |
| evidence script | 新增 `generate_tedq_row_source_family_summary.py`，按同一 RVV binary 计算 family B/A。 |
| board evidence | 5 runs、每次 20 iterations、5 warm-up；checksum 一致；doctor clean。 |
| production boundary | production TEDQ header 未修改，未创建 `doc-rvv`，未重开 PI1。 |

## 证据路径

- summary：`log/board/row_source_family_comparison_repeated/summary.md`
- manifest：`log/board/row_source_family_comparison_repeated/evidence_manifest.json`
- doctor：`log/board/row_source_family_comparison_repeated/evidence_doctor.md`
- registry：`log/evidence_registry.json`

原始 `run-*/run_*.log` 保持 ignored-local，遵循 summary-only evidence policy。

## 解释与边界

direct gather 的正向信号说明，在当前 dense `PointXYZ`、float x/y/z AoS、source-indexed
row pairing 和既有 C1/C2 + solve pipeline 下，绕过 materialize staging 的输入方式值得保留为
后续候选。它没有证明任意索引分布、稀疏点云、其它 point type、dual-indexed 或
correspondence 语义都能获得相同收益。

本阶段没有做 production asm attribution，也没有修改 public dispatch；因此不能写成
production-ready。下一步若要重新进入 production integration loop，仍必须先取得 PI1 授权，
并按 Phase 004 的 path-hit、fallback、production symbol asm、board repeated 和 registry
freshness 合同重新验证。
