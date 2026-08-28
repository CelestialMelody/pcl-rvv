# sac_model_stick optimization evidence

## 本文职责

本文把已尝试的 RVV candidate family（候选族）映射到代码、测试、bench（性能测试）、board summary（板卡摘要）、asm attribution（反汇编归属）、Evidence Doctor（证据体检）和当前 decision（决策）。跨阶段未来搜索空间仍以 `doc/optimization-roadmap.zh.md` 为主；本文只记录已经形成证据或已采纳的路线。

## 当前结论摘要

Phase 080 已把三个 test-only diagnostic（测试专用诊断）候选迁入真实 production public entry（生产公开入口），并完成 post-integration board repeated（接入后板卡重复采集）。Phase 100 又把 `getDistancesToModelRVV` 的 staged scalar lane（暂存后逐向量通道标量写回）替换为 RVV mask / merge（掩码 / 合并）和 `vse64.v` 向量写回，并完成接入后的 public getDistances board repeated。当前 adopted production behavior（已采纳生产行为）覆盖 `countWithinDistance`、`selectWithinDistance` 和 `getDistancesToModel` 三个入口。

## 优化方式总表

| candidate family | 代码路径 | correctness | board evidence | asm evidence | decision | 边界 |
| --- | --- | --- | --- | --- | --- | --- |
| `count-indexed-gather-f32m2-dual-count` | `include/impl/sac_model_stick_diagnostic.hpp` | 历史 diagnostic correctness。 | Phase 000 candidate 4.1429x / 4.1767x / 4.2169x。 | `countWithinDistanceCandidateRVV` 有 FMA、mask、dual popcount。 | adopted via Phase 080 production path | 诊断证据只作为生产接入输入；当前结论以 Phase 080 public entry 数据为准。 |
| `select-indexed-gather-f32m2-compress` | 同上 | 历史 diagnostic correctness。 | Phase 020 candidate 3.4208x / 3.4432x / 3.4761x。 | `selectWithinDistanceCandidateRVV` 有 FMA、mask、`vcompress`。 | adopted via Phase 080 production path | 诊断证据只作为生产接入输入。 |
| `getDistances-indexed-gather-f32m2-sqrt-store` | 同上 | 历史 diagnostic correctness。 | Phase 040 candidate 2.5416x / 2.5553x / 2.7849x。 | `getDistancesToModelCandidateRVV` 有 indexed load、FMA、`vfsqrt.v`、store。 | adopted via Phase 080 production path | 采纳结论来自接入后的 public `getDistancesToModel` 数据。 |
| `production-stick-three-entry-rvv` | `sample_consensus/include/pcl/sample_consensus/sac_model_stick.h`、`impl/sac_model_stick.hpp` | `run_test_compare` Std/RVV 各 11/11。 | Phase 080 public count/select/getDistances median 4.1729x / 3.3023x / 2.5883x。 | `countWithinDistanceRVV`、`selectWithinDistanceRVV`、`getDistancesToModelRVV` 均通过 production asm gate。 | production-adopted | 性能证据覆盖 `PointXYZ` direct indexed board case；代表点型 correctness 由 Phase 090 覆盖。 |
| `point-type-expansion` | `src/test_sac_model_stick.cpp` | `AdditionalAoSPointTypesMatchStandardPath`；focused RVV alias 1/1，full aggregate 11/11。 | not_applicable：本阶段不做 dedicated board performance。 | 使用同一 production helper family，Phase 080 asm 归属仍有效。 | adopted for representative correctness | 覆盖 `PointXYZI`、`PointXYZRGB`、`PointXYZRGBA` 和 `PointXYZRGBNormal` 的 public vs Standard 输出一致；不证明这些点型的性能。 |
| `getDistances-vector-penalty-writeback` | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_stick.hpp`、`script/check_stick_production_asm.py` | focused getDistances RVV 3/3；Std/RVV aggregate 11/11 + 11/11。 | Phase 100 public getDistances min / median / max 3.4122x / 3.6879x / 3.7150x。 | 源码形态 gate 要求无 staged lane loop，asm 要求 `vfwcvt.f.f.v` 和 `vse64.v`。 | production-adopted for current implementation shape | Phase 080 旧数据只作 historical baseline，不写成严格旧/新 RVV A/B。 |

## 标量路径与 RVV 路径差异

public entry 现在只保留模型有效性检查、RVV short-circuit（短路分流）和 Standard fallback（标量回退）调用。原标量主体拆入 `getDistancesToModelStandard`、`selectWithinDistanceStandard` 和 `countWithinDistanceStandard`。

RVV 路径使用 indexed gather（索引离散加载）读取 xyz，批量计算 cross product（叉积）平方距离：

- count 用两个 mask（掩码）和两次 `vcpop.m`，最后保持 `nr_i <= nr_o ? 0 : nr_i - nr_o`。
- select 用 `vcompress.vm` 保序压缩原始 index，并把压缩平方距离转换成 `double` 写入 `error_sqr_dists_`。
- getDistances 用 `vfsqrt.v` 计算距离，用 mask / merge 应用 `radius_max_` penalty，再通过 `vfwcvt.f.f.v` 和 `vse64.v` 写 `std::vector<double>`。

## 证据索引

| 证据 | 路径 / 命令 | 用途 |
| --- | --- | --- |
| correctness | `make -C test-rvv/sample_consensus/sac_model_stick run_test_compare` | 证明 Std/RVV 构建下 candidate 回归、public production direct 输出语义和代表点型 correctness。 |
| point type alias | `make -C test-rvv/sample_consensus/sac_model_stick run_stick_point_type_tests` | 单独验证 `PointXYZI`、RGB/RGBA 和 RGBNormal 代表点型。 |
| bench wrapper | `src/bench_sac_model_stick.cpp` | 生成三条 public entry timing line。 |
| production asm gate | `make -C test-rvv/sample_consensus/sac_model_stick check_production_asm` | 证明生产 helper 命中预期 RVV 指令。 |
| manifest script | `script/generate_stick_board_evidence_manifest.py --focus production` | 生成 Phase 080 Evidence Doctor 输入。 |
| production summary | `doc/phases/080-stick-production-integration/production-repeated-evidence-manifest.json` | Phase 080 repeated board summary。 |
| vector writeback summary | `doc/phases/100-stick-getdistances-vector-writeback/vector-writeback-evidence-manifest.json` | Phase 100 public getDistances repeated board summary。 |
| Evidence Doctor | `doc/phases/080-stick-production-integration/production-repeated-evidence-doctor.md`、`.json` | Phase 080 evidence doctor 0/0/0。 |
| Evidence Doctor | `doc/phases/100-stick-getdistances-vector-writeback/vector-writeback-evidence-doctor.md`、`.json` | Phase 100 evidence doctor 0/0/0。 |
| registry | `log/evidence_registry.json` | 记录 summary evidence 的 freshness。 |

## 结论边界

当前 optimization evidence（优化证据）支持 `production-adopted`。这个结论覆盖接入后的 public entry、`PointXYZ` 代表性板卡性能、direct indexed `indices_`、float xyz AoS 布局和 `Eigen::VectorXf` 系数。Phase 090 进一步覆盖 `PointXYZI`、`PointXYZRGB`、`PointXYZRGBA` 和 `PointXYZRGBNormal` 的 representative correctness（代表性正确性）。它不把这些点型的独立性能、上游 RANSAC 总耗时或其它 sample_consensus topic 写成已证明；这些范围需要单独 phase 和同边界证据。
