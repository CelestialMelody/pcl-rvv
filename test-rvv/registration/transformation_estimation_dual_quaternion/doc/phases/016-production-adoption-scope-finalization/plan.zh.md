# Phase 016 Plan：production adoption scope finalization

## 阶段意图和边界

本阶段执行 PI5 后的用户决策：保留 `ordered-cloud-pair`、`source-indexed-cloud-pair`
和 `dual-indexed-cloud-pair` 三类 production RVV path（生产 RVV 路径），取消
`correspondence-pair` 的 production RVV dispatch（生产分流）。`correspondence-pair`
继续保留 test-rvv 中的诊断候选和历史证据，但生产公开入口回到原 `ConstCloudIterator`
标量路径。

本阶段不扩大 public API（公开接口）、点类型、`Scalar` 或 row-source policy（行来源策略）
范围；不创建 commit。阶段结束停在提交前，让用户判断提交或取消接入。

## 当前状态清单

| area | 当前事实 | 路径 |
| --- | --- | --- |
| Phase 015 production probe | 四类 row source 已接入 production probe；前三类 positive clean，`correspondence-pair` negative 且 doctor 有 Error。 | `doc/phases/015-production-integration-all-row-sources/result.zh.md` |
| production header | 当前仍含四类 RVV helper 和四类 public overload dispatch。 | `registration/include/pcl/registration/impl/transformation_estimation_dual_quaternion.hpp` |
| production tests | RVV 构建当前含四类 path-hit 测试。 | `src/test_tedq.cpp` |
| production bench | `production-public-row-sources` 当前覆盖四类 public entry。 | `src/bench_tedq.cpp`、`Makefile` |
| evidence registry | 已登记 Phase 015 四类 production public evidence。 | `log/evidence_registry.json` |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| production direct C1/C2 RVV reduction | `ordered-cloud-pair` | `PointXYZ` / `float` / dense xyz AoS | public overload | `run_test_compare` + path-hit | retained production public bench | retained repeated board | production public symbol | required clean | retain |
| production direct C1/C2 RVV reduction | `source-indexed-cloud-pair` | `PointXYZ` / `float` / dense xyz AoS + valid source indices | public overload | `run_test_compare` + path-hit + fallback | retained production public bench | retained repeated board | production public symbol | required clean | retain |
| production direct C1/C2 RVV reduction | `dual-indexed-cloud-pair` | `PointXYZ` / `float` / dense xyz AoS + valid dual indices | public overload | `run_test_compare` + path-hit + fallback | retained production public bench | retained repeated board | production public symbol | required clean | retain |
| production correspondence direct index stream | `correspondence-pair` | `PointXYZ` / `float` / dense xyz AoS + correspondences | public overload | public scalar compatibility only | excluded from retained production bench | Phase 015 historical negative | not retained | Phase 015 Error | remove production dispatch |

## 实现和测试动作

| action | 产物 | 完成判据 |
| --- | --- | --- |
| 移除 production correspondence RVV dispatch | production header | `correspondence-pair` public overload 不再调用 RVV helper；无未使用 correspondence-only production RVV helper。 |
| 调整 production RVV tests | `src/test_tedq.cpp` | 保留前三类 path-hit；删除 correspondence path-hit；保留 public correspondence scalar-compatible 回归。 |
| 调整 retained-only bench | `src/bench_tedq.cpp`、`Makefile`、相关 manifest target | 新增或改用 `production-public-retained-row-sources`，只覆盖前三类 retained production path。 |
| 重跑本地验证 | Make targets | Std/RVV correctness 通过；QEMU smoke checksum 一致；asm 归因闭合；doctor clean 或有解释。 |
| 重跑板卡验证 | board target | retained 三类 repeated board positive，Evidence Doctor clean；registry fresh。 |
| 同步文档 | phase result、matrix、roadmap、evaluation、README、适用的 `doc-rvv` | 当前 adopted production 行为只写前三类；correspondence 写为 production RVV not retained。 |

## Evidence Doctor 和 registry 规则

retained production evidence 使用 `production-public-retained-row-sources` 作为 case-filter。若脚本或
registry target 仍引用 `correspondence-pair` 作为 retained production evidence，必须修正后再下结论。

Evidence Doctor（证据体检）要求：

- retained 三类 production public summary：`Errors=0` 才能进入提交前采用结论。
- QEMU smoke：只证明 correctness / log-shape（正确性 / 日志形状），不写性能结论。
- 若 board run 出现 warning，但三类 decision bucket 稳定 positive，result 必须解释风险和边界。

## 板卡复跑预算和决策桶

- 使用现有 `TEDQ_BOARD_REPEATED_RUNS`、`TEDQ_BOARD_BENCH_ITERATIONS` 和
  `TEDQ_BOARD_BENCH_WARMUP_ITERATIONS`。
- 默认一轮 retained repeated board；若 Evidence Doctor Error 或方向反转，再做一次同边界确认复跑。
- bucket：三类均稳定大于 1 且无 Error 为 `positive`；任一 retained policy negative 或 Error 为
  `blocked_for_user_decision`。

## Diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | Phase 015 和本阶段 retained summary 均为 `production-public` / `production_direct`。 |
| A/B boundary | public overload（公开入口）；Std build public scalar path vs RVV build current production dispatch。 |
| 当前决策问题 | RVV-vs-scalar retained production adoption scope。 |
| diagnostic 是否可外推到 production | 不外推；本阶段只使用 production public evidence 作为保留判断。 |
| comparison-boundary / baseline mismatch 风险 | retained 三类使用同 public overload；correspondence 的 Phase 015 negative 不外推到前三类。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | retained 三类若转弱或转负则停止；correspondence 不再保留 production probe。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 当前决策不是新 family selection；判断为当前 public RVV path 是否优于当前 public scalar path。 |

## 文档更新清单

- `doc/phases/016-production-adoption-scope-finalization/result.zh.md`
- `doc/phases/README.zh.md`
- `doc/phases/optimization-matrix.zh.md`
- `doc/optimization-roadmap.zh.md`
- `doc/transformation_estimation_dual_quaternion-evaluation.zh.md`
- `doc/benchmark-and-evidence.zh.md`
- `doc/optimization-evidence.zh.md`
- `doc/testing-overview.zh.md`
- `README.zh.md`
- 适用时更新 `doc-rvv/registration/transformation_estimation_dual_quaternion-RVV.zh.md`

## 继续 / 停止条件

继续到提交前，直到代码、测试、QEMU smoke、asm、板卡 retained evidence、Evidence Doctor、registry
和文档全部同步。停止条件是：

- retained 三类证据出现 Error / negative，需要用户判断；
- 板卡或工具不可用；
- dirty isolation 不安全；
- 全部验证通过后，停在“是否提交或取消接入”的用户判断点。
