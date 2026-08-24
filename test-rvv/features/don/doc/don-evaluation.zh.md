# DON 函数级评估

## 当前结论

`features/include/pcl/features/impl/don.hpp` 的 `DifferenceOfNormalsEstimation::computeFeature()` 是直接逐点热点：读取 small / large normal cloud 的三分量，写出 `0.5 * (small - large)`，非有限输出置零，再把 curvature 写成输出 normal 的欧氏范数。当前 test-only RVV candidate（仅测试使用的 RVV 候选）已通过 QEMU correctness（QEMU 正确性验证）、board correctness、反汇编归属和 1-run board smoke。

当前 EvidenceDecision（证据决策）为 `stop_no_worthwhile_production_direction`：helper-only diagnostic（诊断）5-run repeated board（重复板卡测试）曾是稳定 `weak_positive`，但 production direct（真实生产路径证据）在公开入口边界下为 `negative`。用户已确认回滚，当前 production 源码不再包含 DON RVV helper / dispatch。phase 050 又完成 finite mask / sqrt / curvature-store 消融；没有保持当前 production 语义且值得继续推进的 RVV production 方向。

## 标量路径评估

| 项目 | 说明 |
| --- | --- |
| public entry（公开入口） | `Feature::compute()` 调用 `initCompute()` 后进入 `computeFeature(PointCloudOut&)` |
| 输入检查 | `initCompute()` 要求 small / large normal cloud 存在，且大小等于 `input_->size()` |
| 热点循环 | `computeFeature()` 按 `input_->size()` 顺序遍历 |
| 输出语义 | 写 `normal_x/y/z`，若任一输出分量非有限则三分量置零，随后写 curvature |
| 可 RVV 化片段 | 三分量差、finite mask（有限值掩码）、置零、curvature 平方和和写回 |
| 保留标量片段 | 公开入口对象状态、尺寸检查、未来 production fallback、前置 normal estimation |

## Traceability Map（可追踪性地图）

| 符号 / 文件 | 层级 | 作用 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- |
| `DifferenceOfNormalsEstimation::computeFeature()` | production public entry | 当前标量热点来源 | production boundary（生产边界） | `features/include/pcl/features/impl/don.hpp` |
| `DifferenceOfNormalsEstimation::initCompute()` | production public entry | 调用 `PCLBase<PointInT>::initCompute()` 初始化公开入口状态 | standalone correctness fix（独立正确性修复） | `features/include/pcl/features/impl/don.hpp` |
| `computeDoNScalar()` | diagnostic reference | 复刻 production 标量逐点语义 | correctness reference（正确性参考） | `test-rvv/features/don/include/impl/don_core.hpp` |
| `computeDoNRVV()` | candidate helper | `__RVV10__` 下调用 RVV kernel，非 RVV build 回退 scalar helper | diagnostic candidate（诊断候选） | `test-rvv/features/don/include/impl/don_core.hpp` |
| `test_don.cpp` | correctness test | 对拍期望值、production estimator 和 tail | QEMU / board correctness | `test-rvv/features/don/src/test_don.cpp` |
| `bench_don.cpp` | bench wrapper | 测 helper-only `don_normal_pair` | diagnostic performance | `test-rvv/features/don/src/bench_don.cpp` |
| `bench_don_production.cpp` | production bench wrapper | 经 `Feature::compute()` 测真实 public entry | production-public performance | `test-rvv/features/don/src/bench_don_production.cpp` |
| `generate_don_evidence_manifest.py` | analysis script | 生成 1-run smoke manifest | Evidence Doctor input（证据体检输入） | `test-rvv/features/don/script/generate_don_evidence_manifest.py` |
| `generate_don_repeated_summary.py` | analysis script | 生成 repeated summary 和 manifest | repeated board diagnostic | `test-rvv/features/don/script/generate_don_repeated_summary.py` |

## 诊断证据链

| 证据 | 路径 | 当前含义 |
| --- | --- | --- |
| QEMU Std correctness | `test-rvv/features/don/log/qemu/run_test_std.log` | Std build 3 个 DON gtest 通过 |
| QEMU RVV correctness | `test-rvv/features/don/log/qemu/run_test_rvv.log` | RVV build 3 个 DON gtest 通过 |
| board correctness | `test-rvv/features/don/log/board/run_test.log` | 板卡 RVV build 3 个 DON gtest 通过 |
| asm attribution（反汇编归属） | `test-rvv/features/don/build/asm/riscv/bench_don_rvv.asm` | bench hot path 有跨步加载、向量差、mask、merge、sqrt 和跨步写回 |
| board smoke summary | `test-rvv/features/don/log/board/analyze_bench_compare.log` | 1-run helper-only `1.10x`，只能作为弱正向触发证据 |
| smoke Evidence Doctor | `test-rvv/features/don/log/board/evidence_doctor.md` | Errors=0，Warnings=1（`low_run_count`），Suggestions=0 |
| repeated board summary | `test-rvv/features/don/log/board/repeated_phase010_diagnostic/summary.md` | 5-run median `1.087x`，min `1.069x`，max `1.135x`，`B/A < 1 = 0/5`，decision bucket 为 `weak_positive` |
| repeated Evidence Doctor | `test-rvv/features/don/log/board/repeated_phase010_diagnostic/evidence_doctor.md` | Errors=0，Warnings=0，Suggestions=0 |
| registry | `test-rvv/features/don/log/evidence_registry.json` | 登记当前 board summary / manifest / doctor |

## Production direct 证据链

| 证据 | 路径 | 当前含义 |
| --- | --- | --- |
| QEMU Std/RVV correctness | `test-rvv/features/don/log/qemu/run_test_std.log`, `test-rvv/features/don/log/qemu/run_test_rvv.log` | Std/RVV 各 5 个 gtest 通过，包含 exact normal public entry 和非 exact output fallback |
| QEMU production smoke | `test-rvv/features/don/log/qemu/run_bench_production_rvv.log` | production bench 可运行，QEMU timing 不作为性能结论 |
| production asm attribution | `test-rvv/features/don/build/asm/riscv/bench_don_production_rvv.asm` | phase 030 曾命中 `vlse32/vfsub/vmerge/vfsqrt/vsse32` gate；phase 040 回滚后该 gate 预期失败 |
| production repeated board summary | `test-rvv/features/don/log/board/repeated_phase030_production_direct/summary.md` | 5-run median `0.908x`，min `0.899x`，max `0.949x`，`B/A < 1 = 5/5`，decision bucket 为 `negative` |
| production Evidence Doctor | `test-rvv/features/don/log/board/repeated_phase030_production_direct/evidence_doctor.md` | Errors=1（`ba_degradation_frequency`），Warnings=0，Suggestions=0 |
| detail ablation summary | `test-rvv/features/don/log/board/repeated_phase050_detail_ablation/summary.md` | finite-only no-mask median `0.721x`；no-sqrt zero-curvature median `1.125x` 但破坏 curvature 语义；normal-only median `1.025x` near-threshold |
| detail ablation Evidence Doctor | `test-rvv/features/don/log/board/repeated_phase050_detail_ablation/evidence_doctor.md` | Errors=1，Warnings=3，Suggestions=1 |
| registry | `test-rvv/features/don/log/evidence_registry.json` | production summary / manifest / doctor 已登记且 fresh |

## Production 接入判断

当前有界 production patch 已按用户确认回滚：`computeFeature()` 回到原标量逐点循环，DON production 源码不再按 `__RVV10__` 分流到 RVV。production-public board evidence 不支持采纳该 RVV path，因此当前生产接入判断是 no-production（不接入生产）。

phase 050 表明 `sqrt` 是主要成本中心之一，但去掉 `sqrt` 会改变 curvature 输出语义。当前没有保持 strict `sqrt` 语义且稳定 positive 的 RVV route；除非用户授权 approximate sqrt（近似平方根）误差预算或未来引入可验证的 faster sqrt helper，否则 DON topic 暂停，不继续接 production。

本阶段还发现并修复了真实 `Feature::compute()` 入口下 `indices_` 未初始化的问题：DON `initCompute()` 现在调用 `PCLBase::initCompute()`，但不调用 `Feature::initCompute()`，避免引入 DON 不需要的 K/radius 搜索参数要求。该初始化修复可以与 RVV 分流是否保留分开评审。
