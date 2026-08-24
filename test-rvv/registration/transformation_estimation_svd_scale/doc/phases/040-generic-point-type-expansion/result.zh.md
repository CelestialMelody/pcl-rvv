# Phase 040 结果：generic point type expansion correctness scout

## EvidenceDecision

`correctness_scout_positive_board_pending`。

本阶段已把 `generic-point-type-expansion` 从 PI5 blocked 状态推进到 correctness scout：当前 production public path 在代表 xyz AoS 点型上与同构标量 reference 对齐。该结果不把 generic point type 写成完整 production performance adoption；board repeated 和 ASM attribution 仍是下一阶段未闭合项。

## 实际修改

| 文件 | 修改 | 证据角色 |
| --- | --- | --- |
| `include/impl/tesvd_scale_support.hpp` | 新增泛型 xyz 点云构造和跨点型 transform helper。 | 让代表点型使用同一 deterministic 样本。 |
| `include/impl/tesvd_scale_candidates.hpp` | 将 `accumulateScaleStd`、`estimateScaleStd` 和 `estimateScalePublic` 模板化。 | 让 public path 与同构标量 reference 可以对拍不同 `PointSource` / `PointTarget`。 |
| `src/test_tesvd_scale.cpp` | 新增 `GenericXYZPointTypesMatchReference`。 | production public correctness。 |

## correctness 结果

| target | result |
| --- | --- |
| `make -C test-rvv/registration/transformation_estimation_svd_scale run_test_compare` | Std/RVV 8 tests passed。 |

新增测试覆盖：

- `PointXYZI -> PointXYZI`
- `PointXYZRGB -> PointXYZRGB`
- `PointXYZI -> PointXYZRGB`
- `PointXYZRGB -> PointXYZ`

每个组合都要求 `RVVXYZAoSFloatLayout<PointSource/PointTarget>::value` 为 true，并比较 public output 与 `estimateScaleStd` reference，误差预算为 `5e-4`。

## 证据边界

| 维度 | 结论 |
| --- | --- |
| production source | 未修改；本阶段只验证当前已采纳 traits-gated dispatch。 |
| correctness | 正向。 |
| QEMU smoke / bench | 未运行；不写性能结论。 |
| ASM attribution | 未刷新；不能声称 generic point type 符号内已完成归因。 |
| board repeated | 未运行；generic point type 性能仍 pending。 |
| all generic point types | 未关闭；当前只覆盖代表点型组合。 |

## optimization matrix 更新

`generic-point-type-expansion` 从 `not_started / blocked_by_PI5` 更新为 `correctness_scout_positive_board_pending`。下一步若继续推进，应新增 generic point type case-filter 或 board repeated target，并补 ASM attribution 与 Evidence Doctor。

## 下一步

默认恢复入口是 generic point type board / ASM phase：为代表点型增加可隔离 bench case-filter，运行 QEMU smoke、production symbol attribution 和 board repeated summary。source-indexed / dual-indexed / correspondence 仍保持独立队列。
