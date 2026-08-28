# sac_model_sphere 测试支撑代码地图

## 总调用图

| 符号 / 文件 | 层级 | 作用 | 上游入口 | 证据角色 |
| --- | --- | --- | --- | --- |
| `countWithinDistance` | production public entry | 公开计数入口，RVV 构建可命中 `countWithinDistanceRVV`。 | PCL SAC callers | production direct boundary。 |
| `selectWithinDistance` | production public entry | 公开 inlier 选择入口，RVV 构建可命中 `selectWithinDistanceRVV`。 | PCL SAC callers | production direct boundary。 |
| `getDistancesToModel` | production public entry | 公开距离数组入口，当前保持标量。 | PCL SAC callers | scalar public baseline。 |
| `include/impl/sac_model_sphere_access.hpp` / `SampleConsensusModelSphereAccess` | internal test support | 暴露 Standard / RVV helper，并提供 test-only select/getDistances candidate。 | gtest / bench 聚合头 | production direct / production-shaped diagnostic。 |
| `include/impl/sac_model_sphere_access.hpp` / `sqrDistancesRVV` | candidate formula | RVV 计算 xyz gather 后的平方距离。 | test-only candidates | RVV formula evidence。 |
| `include/test_sac_model_sphere.h` | correctness aggregator | 保存 fixture、系数构造和断言 helper。 | `src/test_sac_model_sphere.cpp` | correctness support。 |
| `include/bench_sac_model_sphere.h` | bench aggregator | 计时 public entries 和 test-only candidates，处理点型分发。 | `src/bench_sac_model_sphere.cpp` | board performance input。 |
| `src/test_sac_model_sphere.cpp` | test entry | 保留 gtest case 和 main。 | `run_test_compare` | correctness gate。 |
| `src/bench_sac_model_sphere.cpp` | bench entry | 保留 CLI 解析和 unsupported point type 返回。 | board bench target | bench executable entry。 |
| `script/generate_sphere_board_evidence_manifest.py` | analysis script | 解析 repeated board logs 生成 diagnostic 或 production manifest。 | `generate_board_evidence_manifest` / `record_production_board_evidence_state` | Evidence Doctor input。 |
| `repeated-evidence-manifest.json` | evidence output summary | 保存 5-run board 数值和 metadata。 | Evidence Doctor | summary evidence。 |
| `log/evidence_registry.json` | evidence registry | 登记 repeated manifest 和 doctor 状态。 | `repeated_evidence_status` | freshness metadata。 |

## Fixtures 与输入构造

测试和 bench 构造 direct indexed shell cloud，使用乱序 `indices_` 迫使路径按 index gather 读取 x/y/z。
当前 correctness 覆盖 `PointXYZ` / `PointXYZI` / `PointXYZRGB` / `PointXYZRGBA`，production board
repeated 覆盖这些内建代表点型；自定义 registered xyz 点型仍未定义代表输入。

## Candidate / Diagnostic Helper

| helper | 位置 | fallback / 状态 | 不能证明 |
| --- | --- | --- | --- |
| `selectWithinDistanceCandidate` | `include/impl/sac_model_sphere_access.hpp` | RVV 构建且 `RVVXYZFloatLayout<PointT>` 成立时走测试专用 RVV，否则走标量 candidate。 | 不能证明 production dispatch 已存在。 |
| `getDistancesToModelCandidate` | `include/impl/sac_model_sphere_access.hpp` | 同上，但当前 board 负向。 | 不能证明所有 future getDistances RVV 都不可行。 |
| `sqrDistancesRVV` | `include/impl/sac_model_sphere_access.hpp` | 只计算平方距离，不负责 sqrt 或输出容器语义。 | 不能替代 select/getDistances 的完整 public entry。 |
| `selectWithinDistanceStandard` | production source | 保存原标量 fallback 语义。 | 不证明 RVV 性能；用于 correctness 对拍和 fallback。 |
| `selectWithinDistanceRVV` | production source | 接管 indexed xyz gather、平方距离和球壳判断。 | 不覆盖 `getDistancesToModel` 或更多点型性能。 |

## Bench Harness 与 Case Registry

Bench case registry 当前写在 `script/generate_sphere_board_evidence_manifest.py` 的 `ITEM_METADATA`。它定义 case name、evidence role、A/B boundary、wrapper、timer boundary、mask、reduction 和 asm symbol。Phase 050/060 后脚本会从 `Dataset:` 解析 `PointXYZI` / `PointXYZRGB` / `PointXYZRGBA`，并让 manifest 的 `point_type`、`name`、`gate` 和 asm attribution 绑定到当前点型实例。

## Scripts 与 Evidence Output

`generate_sphere_board_evidence_manifest.py` 只解析当前 topic 的 repeated board raw logs。production 模式能归属
`selectWithinDistanceRVV` 和 `countWithinDistanceRVV` 符号级 RVV 指令；测试 candidate 因内联导致
`rvv_instr_count=0`，仍只作为诊断证据。

## 拆分审计

Phase 055 前 `src/test_sac_model_sphere.cpp` 约 494 行，`src/bench_sac_model_sphere.cpp` 约 431 行，职责已经包含 fixture、reference、candidate、assertion、bench harness 和 point-type dispatch。用户指出当前结构不同于 `test-rvv/registration/transformation_estimation_point_to_plane_lls/include`；`.agents/config/defaults.yaml` 也把聚合头默认目录设为 `include`、内部 helper 目录设为 `include/impl`。

Phase 055 已完成迁移：共享 access / candidate 放在 `include/impl/sac_model_sphere_access.hpp`，test 与 bench 聚合入口放在 `include/`。迁移后通过 `run_test_compare`、`run_bench_rvv BENCH_ARGS='128 1 PointXYZI'` 和 `clean_bench_rvv dump_bench_rvv` 验证；Phase 060 又在该结构上扩展 RGB/RGBA 点型。
