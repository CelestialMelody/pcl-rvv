# moment_invariants test support code map

## 本文职责

本文帮助 reviewer（审查者）定位测试支撑代码、production helper（生产 helper）、bench wrapper（性能测试包装）和 evidence output（证据输出）。性能结论主归属仍在 `doc/benchmark-and-evidence.zh.md` 与对应 summary。

## 总调用图

```text
production moment_invariants.hpp
  -> computeFeature
  -> computePointMomentInvariants(cloud, indices, ...)
  -> momentInvariantsIndexedMomentsRVV or momentInvariantsIndexedMomentsStd

test_moment_invariants.cpp
  -> helper-only same-chain tests
  -> production direct public computeFeature tests
  -> non dense fallback test
  -> typed PointXYZI / PointXYZRGB / PointXYZRGBA tests

bench_moment_invariants.cpp
  -> historical helper-only cases
  -> historical public-search-shaped helper replacement
  -> production-public computeFeature cases

generate_mi_repeated_summary.py
  -> summary.md + evidence_manifest.json
evidence_doctor.py
  -> evidence_doctor.md / json
evidence_registry.py
  -> log/evidence_registry.json
```

## 代码地图

| 符号 / 文件 | 层级 | 作用 | 证据角色 |
| --- | --- | --- | --- |
| `features/include/pcl/features/impl/moment_invariants.hpp` | production public/helper | 真实 `computeFeature`、Std helper、RVV helper、dispatch（分流逻辑）和 fallback（回退路径）。 | production boundary（生产边界）。 |
| `momentInvariantsIndexedMomentsRVV` | production RVV helper | `RVVXYZAoSFloatLayout<PointT>` gate 后按 indices gather xyz 并规约六个中心矩。 | production direct RVV path。 |
| `momentInvariantsIndexedMomentsStd` | production Std helper | 保留原 indexed 标量循环。 | fallback / reference path（回退 / 参考链路）。 |
| `momentInvariantsFullMomentsStd` | production Std helper | full-cloud overload 的标量实现。 | scalar-only boundary。 |
| `include/moment_invariants.h` | aggregator header（聚合头） | 暴露测试专用 helper 入口。 | test support include boundary。 |
| `include/impl/moment_invariants_reductions.hpp` | diagnostic reference / candidate helper | Std reference、测试专用 RVV gather/stride load、vector reduction。 | helper-only correctness、historical asm、diagnostic bench。 |
| `src/test_moment_invariants.cpp` | correctness test source | 构造合成点云、稀疏 indices、public KdTree 查询、非 dense fallback 和 typed output 对拍。 | QEMU / board correctness。 |
| `src/bench_moment_invariants.cpp` | bench wrapper | 实现 helper-only、production-shaped 和 production-public case-filter、checksum 和计时输出。 | board repeated 输入。 |
| `script/check_mi_production_asm.py` | asm attribution script | 按 production symbol fragment 查找 `vlux*ei32` 和 `vfred*sum`。 | production asm attribution（生产反汇编归属）。 |
| `script/generate_mi_repeated_summary.py` | analysis script（分析脚本） | 解析 repeated run、生成 summary 和 manifest。 | Evidence Doctor 输入。 |
| `Makefile` / `board.mk` | harness（运行框架） | 构建、QEMU、反汇编、板卡采集和 registry target。 | 可复现命令入口。 |
| `log/board/repeated_phase030_production_compute_feature/` | evidence output | `PointXYZ` production-public repeated summary、manifest、doctor。 | production performance evidence。 |
| `log/board/repeated_phase040_production_point*_compute_feature/` | evidence output | `PointXYZI`、`PointXYZRGB`、`PointXYZRGBA` production-public repeated summary、manifest、doctor。 | production performance evidence。 |

## 拆分审计

当前 test support 已使用 `src/`、`include/` 和 `include/impl/`，没有旧 `test_support/` 目录。测试专用 helper header 同时承担 reference、candidate 和 RVV intrinsic 包装，但行数较短，职责都围绕同一个中心矩规约候选；本阶段保留单个 internal header。

Production helper 已放在目标 production header 中，不复用 test-only helper。bench 和 correctness 分别位于 `src/bench_moment_invariants.cpp` 与 `src/test_moment_invariants.cpp`，summary/doctor/registry 由 topic-local script 和通用 evidence scripts 串接。当前没有未阻塞的 test-source-split 或 internal-helper-layout 动作。
