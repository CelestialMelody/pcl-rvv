# Phase 025 Result: empty indices correctness gap

## 执行范围

本阶段只关闭显式空 `indices_` 的 correctness（正确性）覆盖缺口。它不修改 production（生产源码）
RVV helper，不新增 bench（性能测试）或 repeated board performance（重复板卡性能）结论。

| 维度 | 已验证范围 | 未验证范围 |
| --- | --- | --- |
| indices | 显式 `setIndices(std::make_shared<std::vector<int>>())` 的空子集。 | correspondence、双索引和其它 row source policy。 |
| 点型 | `PointXYZ`。 | 用户自定义点型全集；代表点型性能仍不外推。 |
| 入口 | `selectWithinDistance`、`countWithinDistance`、`getDistancesToModel` public entry 与 Standard helper 对拍。 | 其它 SAC 模型和其它模型后处理。 |
| 性能 | 不新增性能结论。 | 空子集没有 meaningful performance（有意义性能）判断。 |

## 计划动作回填

| action | 状态 | 命令 / 路径 | 结论 |
| --- | --- | --- | --- |
| 新增显式空 `indices_` gtest | done | `src/test_sac_model_plane.cpp` 的 `ExplicitEmptyIndicesMatchStandardPath` | public entry 输出空 inliers、count 为 0、distances 为空，且不会保留旧输出。 |
| 更新 public filter | done | `Makefile` 的 `BASE_PLANE_PUBLIC_FILTER` | 板卡 public correctness alias 已包含新增 case。 |
| QEMU correctness | done | `make -C test-rvv/sample_consensus/sac_model_plane run_test_compare` | Std/RVV 两个构建各 7 个 gtest 通过。 |
| board correctness smoke | done with environment warning | `SSH_AUTH_SOCK=<agent-socket> make -C test-rvv/sample_consensus/sac_model_plane run_board_base_plane_public_tests REMOTE_USER=<board-user> REMOTE_IP=<board-ip>` | 板卡 RVV gtest 7/7 通过；Makefile clock skew 是板卡环境 warning。 |
| docs / matrix update | done | README、correctness、testing overview、optimization evidence、roadmap、matrix、evaluation、`doc-rvv`、Handoff | 当前 correctness suite 和板卡 public smoke 均记录为 7/7，命令示例已脱敏。 |

## EvidenceDecision

`current_decision=adopted_for_correctness`。

显式空 `indices_` 与默认 cloud-only identity path（整云恒等索引路径）不同，当前测试已经单独覆盖。
该 case 证明三条 public entry 在空子集下会清空输出并与 Standard helper 保持一致。它不改变 Phase 000/010
的 RVV performance（性能）采纳结论，也不支持新的生产优化方式。

## Evidence Doctor 和 registry

本阶段没有新增 repeated board performance 数据，因此不生成新的 Evidence Doctor manifest（证据体检清单）。
Phase 000/010 的性能 Evidence Doctor 结果继续作为性能证据；Phase 025 只更新 correctness 证据边界。

## 继续 / 停止判断

`continue_stop_decision=stop_current_performance_search`。

当前 topic 内已闭合：

- Phase 000：三入口 gather RVV production adopted。
- Phase 010：select/count identity strided load narrow adopted；getDistances identity branch rejected。
- Phase 020：代表点型 correctness adopted。
- Phase 025：显式空 `indices_` correctness adopted。

当前没有新的未阻塞 RVV performance candidate（性能候选）。剩余 `030-evidence-registry-hardening`
是 evidence registry / environment metadata（证据登记 / 环境元数据）硬化，服务提交和长期归档，
不改变性能采纳结论。
