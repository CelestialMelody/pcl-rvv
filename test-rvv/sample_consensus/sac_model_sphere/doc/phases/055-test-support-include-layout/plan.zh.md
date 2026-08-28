# Phase 055: test support include / impl 布局计划

## 阶段意图和边界

用户指出当前 topic 的测试支撑代码没有采用类似 `test-rvv/registration/transformation_estimation_point_to_plane_lls/include` 的结构。审计后确认：`.agents/config/defaults.yaml` 记录了 `test_support.aggregator_directory=include` 和 `test_support.internal_directory=include/impl`；`rvv-test/references/optimization-phase-loop.zh.md` 要求在复杂 topic 或 mature sibling（成熟相邻主题）可校准时，把聚合头、内部 helper 布局作为 structure parity（结构对齐）审计项。

Phase 010 曾因 test / bench 文件还不大而暂缓拆分，只补齐文档套件。Phase 050 后 `src/bench_sac_model_sphere.cpp` 已加入点型选择、manifest 点型解析和更多证据 target；继续把 fixture、candidate、assertion 和 bench harness 堆在 `src/*.cpp` 会增加后续 RGB/RGBA 点型扩展的 reviewer 负担。因此本阶段迁移测试支撑结构。

本阶段不修改 production 源码，不改变 gtest case 名、bench 输出字段、Make target 名、Evidence Doctor 输入语义或 Phase 045/050 数值结论。

## 当前 shape scan

| area | current shape | 目标 |
| --- | --- | --- |
| test source | `src/test_sac_model_sphere.cpp` 约 494 行，包含 access wrapper、fixture、reference、candidate、assertion 和 gtest。 | `src/` 只保留 gtest 入口和少量测试编排。 |
| bench source | `src/bench_sac_model_sphere.cpp` 约 431 行，包含 bench wrapper、candidate、fixture、point-type dispatch 和 main。 | `src/` 只保留 main / CLI；bench 支撑放入 `include/bench_sac_model_sphere.h` 和 `include/impl/*`。 |
| include layout | 当前无 topic-local `include/`。 | 新增聚合头和 `include/impl` 内部 helper。 |
| sibling calibration | `registration/transformation_estimation_point_to_plane_lls` 使用 `include/teptpl.h`、`include/test_teptpl.h`、`include/bench_teptpl.h` 和多个 `include/impl/teptpl_*.hpp`。 | 采用同类职责边界，不复制文件名或算法。 |

## 实现计划

| 动作 | 产物 | 完成判据 |
| --- | --- | --- |
| 建立聚合头 | `include/test_sac_model_sphere.h`、`include/bench_sac_model_sphere.h` | src 文件能通过聚合头找到 test / bench 支撑。 |
| 拆共享 access / candidate helper | `include/impl/sac_model_sphere_access.hpp` | test 和 bench 共用同一 access wrapper，gtest case 名和断言语义不变。 |
| 拆 bench harness | `include/bench_sac_model_sphere.h` | bench 默认输出和 `PointXYZI` 第三个参数输出不变。 |
| 更新 Makefile include path | `Makefile` | test / bench 编译能找到 topic-local include。 |
| 文档同步 | code map、roadmap、matrix、Phase 055 result、Handoff | 明确回答为什么之前不是该结构、skill 在哪里记录、当前如何修正。 |

## 验证

```bash
make -C test-rvv/sample_consensus/sac_model_sphere run_test_compare
make -C test-rvv/sample_consensus/sac_model_sphere run_bench_rvv BENCH_ARGS='128 1 PointXYZI'
make -C test-rvv/sample_consensus/sac_model_sphere clean_bench_rvv dump_bench_rvv
```

本阶段是 test support refactor（测试支撑重构），不产生新 board 性能结论；Phase 050 board evidence 已经闭合。若重构后 bench 输出改变、gtest 数量改变或 manifest 无法解析，则阶段不能关闭。

## Continue / Stop Decision

若本阶段通过验证，`PointXYZRGB` / `PointXYZRGBA` 是否继续扩展再由 roadmap 判断；若结构迁移引入无法快速消除的编译失败，回到当前 topic-local src 结构并记录 blocker。
