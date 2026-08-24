# Phase 080 Plan: full rollback no-adoption closeout

## 阶段意图和边界

本阶段执行用户确认的建议路径：完整回滚 `io/include/pcl/compression/color_coding.h` 中剩余的 `setDefaultColor` RVV production 分流，把 `color_coding` topic 收口为 no-adoption（不采纳生产 RVV）状态。

本阶段覆盖：

- 生产文件：`io/include/pcl/compression/color_coding.h`。
- 测试资产：`test-rvv/io/color_coding/src/test_color_coding.cpp`、`src/bench_color_coding.cpp` 和相关文档。
- 文档：Phase 080 result、phase index、optimization matrix / roadmap、evaluation、README、benchmark / correctness / optimization evidence、handoff、筛选清单。

本阶段不覆盖：

- 新的 RVV implementation family（实现族）。
- 新板卡性能复跑。Phase 070 已完成 default-only production-public 证据并触发停止条件；完整回滚后没有 production RVV 分流需要性能比较。
- `doc-rvv/io/color_coding-RVV.zh.md`。当前没有 adopted production behavior（已采用生产行为）。

## 当前状态清单

| item | current state | path |
| --- | --- | --- |
| Phase 070 result | default-only median `1.0035x`、min `0.9945x`，Doctor `Errors=1`；不建议保留 | `doc/phases/070-pi5-partial-rollback-default-only/result.zh.md` |
| production patch | `encodeAverageOfPoints` / `encodePoints` 已回滚；`setDefaultColorRVV` 仍暂留 | `io/include/pcl/compression/color_coding.h` |
| production tests | 仍有 default hook 断言 | `src/test_color_coding.cpp` |
| production bench | `--case-filter production` 只输出 default-only label | `src/bench_color_coding.cpp` |
| evidence registry | phase 070 production evidence fresh | `log/evidence_registry.json` |
| long-term doc | 未创建，且仍不适用 | `doc-rvv/io/color_coding-RVV.zh.md` |

## 优化矩阵

| candidate family | scope and entry | evidence | decision | action |
| --- | --- | --- | --- | --- |
| production encode average RVV | public `encodeAverageOfPoints` | phase 060 negative, Doctor Errors | rejected | already rolled back |
| production encode points average-pass RVV | public `encodePoints` | phase 060 weak / unstable | rejected / not recommended | already rolled back |
| production default color RVV | public `setDefaultColor` | phase 070 neutral / unstable, Doctor Error | rejected / not recommended | full rollback in this phase |
| decode RVV | public `decodePoints` | phase 050 weak / rejected shape | rejected / keep scalar | no action |

## 实现和测试动作

| action | artifact / command | completion criterion |
| --- | --- | --- |
| 移除 production default RVV | `color_coding.h` | 无 `__RVV10__` / `riscv_vector.h` / `setDefaultColorRVV` / `colorRvv*` / RVV hook 残留；公开入口直接调用标量 |
| 更新 production direct tests | `src/test_color_coding.cpp` | 删除 `PCL_COLOR_CODING_RVV_TEST_HOOK` 和 hook 断言；保留真实 public method 语义测试 |
| 更新 production bench | `src/bench_color_coding.cpp` | 删除 `prod_set_default_color_4096` label 或使 `production` filter 无当前 production RVV case；不再产生误导性 Std/RVV production compare |
| 更新 Makefile registry | `Makefile` | production repeated target 不再作为当前 closeout 必跑项；freshness 仍检查 phase 070 historical evidence |
| 本地验证 | `run_test_compare`、`check_evidence_freshness`、`diff --check` | correctness 通过，registry fresh，whitespace clean |

## Evidence Doctor 和 registry 规则

本阶段不生成新的 board production evidence；Phase 070 summary / manifest / Doctor 是当前 no-adoption decision 的 evidence truth（证据事实）。本阶段只跑 `check_evidence_freshness`，确认 historical evidence 仍登记且文档引用不 stale。

## 继续 / 停止条件

继续条件：

- 生产源码仍残留 RVV 分流或 hook。
- 测试 / 文档仍宣称有 production RVV 候选可保留。
- correctness、freshness 或 diff check 未通过。

停止条件：

- 生产源码回到纯标量 `ColorCoding` 行为。
- topic-local 文档和筛选表均写明 no-adoption。
- roadmap / matrix 没有当前授权范围内、未阻塞且建议继续的高优先级优化方向。

默认下一动作：完整回滚剩余 default RVV，并刷新 no-adoption closeout 文档。
