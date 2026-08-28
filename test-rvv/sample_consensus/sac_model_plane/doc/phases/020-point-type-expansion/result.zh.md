# Phase 020 Result: point type expansion

## 执行范围

本阶段只关闭代表性 registered single-float xyz AoS（已注册单精度 xyz 结构数组）点型的
correctness（正确性）和 dispatch（分流）证据。没有生成这些点型的 dedicated board performance
（专门板卡性能）结论。

| 维度 | 已验证范围 | 未验证范围 |
| --- | --- | --- |
| 点型 | `PointXYZI`、`PointXYZRGB`、`PointXYZRGBA`、`PointXYZINormal`。 | 用户自定义点型全集、非 AoS layout、非 float xyz。 |
| indices | shuffled direct indexed 和 identity cloud-only。 | correspondence、双索引和其它 row source policy。 |
| 入口 | `selectWithinDistance`、`countWithinDistance`、`getDistancesToModel` public entry 与 Standard path 对拍。 | 其它 SAC 模型和 SAC 后处理。 |
| 性能 | 不新增性能结论。 | 这些点型的 dedicated board repeated performance。 |

## 计划动作回填

| action | 状态 | 命令 / 路径 | 结论 |
| --- | --- | --- | --- |
| add representative point fixtures | done | `src/test_sac_model_plane.cpp` | 点云构造支持 `PointXYZI`、`PointXYZRGB`、`PointXYZRGBA`、`PointXYZINormal`。 |
| add correctness tests | done | `AdditionalAoSPointTypesMatchStandardPath`、`AdditionalAoSPointTypesIdentityMatchStandardPath` | shuffled 和 identity 两类 indices 都与 Standard path 对拍。 |
| QEMU correctness | done | `make -C test-rvv/sample_consensus/sac_model_plane run_test_compare` | Phase 020 当时 Std/RVV 两个构建各 6 个 gtest 通过；Phase 025 后当前套件已扩展为 7 个 gtest。 |
| board correctness smoke | done with environment warning | `SSH_AUTH_SOCK=<agent-socket> make -C test-rvv/sample_consensus/sac_model_plane run_board_base_plane_public_tests REMOTE_USER=<board-user> REMOTE_IP=<board-ip>` | Phase 020 当时板卡 RVV gtest 6/6 通过；Phase 025 后当前板卡 public correctness 为 7/7；Makefile clock skew 是板卡环境 warning。 |
| docs / matrix update | done | README、evaluation、roadmap、matrix、`doc-rvv` | 文档明确代表点型 correctness 已扩展，性能仍只引用 `PointXYZ` repeated board。 |

## EvidenceDecision

`current_decision=adopted_for_representative_point_type_correctness`。

现有 `RVVXYZAoSFloatLayout<PointT>` gate 在更多常见 AoS 点型上可以编译并通过 public entry
对拍。由于本阶段没有 dedicated board repeated performance，长期文档只能声明这些点型的 correctness /
dispatch 证据已覆盖，不能把 `PointXYZ` 性能外推到这些点型。

## 继续 / 停止判断

`continue_stop_decision=stop_current_performance_search`。

当前 topic 内已尝试并闭合的性能优化是：

- Phase 000：三入口 gather RVV production adopted。
- Phase 010：select/count identity strided load narrow adopted。
- Phase 010 rejected：getDistances identity strided load。

下一项未做的是 evidence registry / environment metadata hardening（证据登记和环境元数据硬化）。它能提升
证据可审查性，但不是新的 RVV performance candidate（性能候选）。因此当前性能优化搜索暂停；若要准备
提交或长期归档 evidence logs，再进入 `030-evidence-registry-hardening`。

Phase 025 已补显式空 `indices_` correctness 缺口；它不改变本阶段的点型性能边界。
