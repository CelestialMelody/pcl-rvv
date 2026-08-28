# Phase 055: test support include / impl 布局结果

## 当前结论

本阶段已采用 `include` / `include/impl` 测试支撑布局，未修改 production 源码，未改变 gtest case 名、bench 输出字段、Make target 名或 Evidence Doctor 输入语义。

用户提出“为什么当前测试代码不是类似 `test-rvv/registration/transformation_estimation_point_to_plane_lls/include` 的形式”。答案是：Phase 010 当时判断 test / bench 文件规模较小，先补齐文档套件并暂缓拆源码 helper；但 `.agents/config/defaults.yaml` 已记录 `test_support.aggregator_directory=include` 和 `test_support.internal_directory=include/impl`，`rvv-test/references/optimization-phase-loop.zh.md` 也要求把 mature sibling（成熟相邻主题）的 `include/impl` 形态作为结构质量门槛校准。Phase 050 增加点型选择和 board evidence 后，这个暂缓项已经变成未阻塞的结构动作，所以本阶段完成迁移。

## 实际改动

| 文件 | 当前职责 |
| --- | --- |
| `include/impl/sac_model_sphere_access.hpp` | 共享 `SampleConsensusModelSphereAccess`，暴露 Standard/RVV helper，并保留测试专用 select/getDistances candidate。 |
| `include/test_sac_model_sphere.h` | correctness tests 聚合头，保存 fixture、系数构造和断言 helper。 |
| `include/bench_sac_model_sphere.h` | bench 聚合头，保存计时 harness、输入构造、点型分发和输出合同。 |
| `src/test_sac_model_sphere.cpp` | 瘦身为 gtest 入口和 5 个 TEST。 |
| `src/bench_sac_model_sphere.cpp` | 瘦身为 CLI 解析和 unsupported point type 错误返回。 |
| `Makefile` | 增加 topic-local `-I$(CURDIR)/include`。 |

迁移后行数扫描：

| 文件 | 行数 |
| --- | ---: |
| `src/test_sac_model_sphere.cpp` | 141 |
| `src/bench_sac_model_sphere.cpp` | 30 |
| `include/test_sac_model_sphere.h` | 85 |
| `include/bench_sac_model_sphere.h` | 154 |
| `include/impl/sac_model_sphere_access.hpp` | 305 |

## 验证

| 命令 | 结果 | 说明 |
| --- | --- | --- |
| `make -C test-rvv/sample_consensus/sac_model_sphere run_test_compare` | passed | Std/RVV 两个构建各 5 个 gtest 通过。 |
| `make -C test-rvv/sample_consensus/sac_model_sphere run_bench_rvv BENCH_ARGS='128 1 PointXYZI'` | passed | QEMU 日志形状保持，Dataset 行为 `PointXYZI`。 |
| `make -C test-rvv/sample_consensus/sac_model_sphere clean_bench_rvv dump_bench_rvv` | passed | RVV bench 可重新构建并生成反汇编。 |

本阶段是测试支撑结构迁移，不产生新的性能数据；Phase 045/046 的 `PointXYZ` production board 数据和 Phase 050 的 `PointXYZI` board 数据仍是当前长期事实。

## Structure Parity 审计

| area | current shape scan | config / quality bar / supplemental calibration | decision | blocker / evidence | next action |
| --- | --- | --- | --- | --- | --- |
| test/bench source layout | `src/test_sac_model_sphere.cpp` 与 `src/bench_sac_model_sphere.cpp` 已瘦身为入口文件。 | `.agents/config/defaults.yaml` 要求 topic-local 聚合入口优先放在 `include`。 | adopted | `run_test_compare` 和 bench smoke 通过。 | 后续新增 case 时继续保持 src 只放编排。 |
| aggregator and internal helpers | 已新增 `include/test_sac_model_sphere.h`、`include/bench_sac_model_sphere.h` 和 `include/impl/sac_model_sphere_access.hpp`。 | mature sibling 的 `include/impl` 只作为职责边界校准，不复制算法和文件名。 | adopted | 聚合头能被 Makefile 编译路径解析。 | 若 helper 超过职责阈值，再拆 fixtures / assertions / bench_cases。 |
| script and bench registry | manifest 脚本、case label、checksum 和 asm 输出合同不变。 | Phase 050 已验证 point type 解析。 | adopted | `PointXYZI` Dataset 行 smoke 通过。 | Phase 060 若扩 RGB/RGBA，复用现有 manifest 解析并新增独立 evidence target。 |
| target granularity | Makefile 现有 correctness、board repeated、doctor / registry target 保持不变。 | 结构迁移不能改 evidence target 合同。 | adopted | `dump_bench_rvv` 和既有 target 编译链通过。 | 下一阶段只在新增点型 evidence 时加 target。 |
| topic-local docs | README、evaluation、roadmap、matrix 和 code map 已同步。 | phase-loop 要求结构缺口不能只写 roadmap。 | adopted | 本 result 固化采用原因和验证。 | 无结构阻塞。 |
| long-term docs | `doc-rvv` 保持只记录 adopted production 行为和证据链。 | 结构迁移不是 production 行为。 | not_applicable with evidence | 不新增 `doc-rvv` 性能结论。 | 下一点型 evidence 若接入后正向，再刷新长期文档。 |
| evidence freshness | 结构迁移不覆盖 board summary evidence。 | summary-only 策略保持。 | adopted | 后续运行 status target 复核。 | final scan 前检查 registry freshness。 |

## Continue / Stop Decision

未命中停止条件。Phase 055 已关闭原先的结构缺口；下一默认阶段是 `060-point-type-rgb-rgba-expansion`，用于给 `PointXYZRGB` / `PointXYZRGBA` 补 dedicated correctness、日志形状、asm 和 board evidence，避免把 `PointXYZ` / `PointXYZI` 的性能结论直接外推到其它 registered xyz 点型。
