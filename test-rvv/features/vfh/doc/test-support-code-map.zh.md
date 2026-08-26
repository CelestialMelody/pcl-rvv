# VFH 测试支撑代码地图

## 文件结构

| 文件 / 目录 | 职责 | 调用者 | 证据边界 |
| --- | --- | --- | --- |
| `Makefile` | 定义 Std/RVV test、bench、asm、board repeated 和 Evidence Doctor target。 | worker / reviewer | target 入口，不保存性能结论。 |
| `board.mk` | 板卡侧二进制和远端执行片段配置。 | board targets | 不提交私有板卡值。 |
| `include/vfh.h` | topic-local 聚合头。 | `src/test_vfh.cpp`、`src/bench_vfh.cpp` | 测试支撑入口。 |
| `include/impl/vfh_reference.hpp` | reference、fixtures 和 test-only RVV candidate。 | gtest、bench | diagnostic correctness / candidate evidence；不等于 production dispatch。 |
| `src/test_vfh.cpp` | correctness gtest。 | `run_test_compare`、`board_smoke` | public reference、candidate 和 production helper gate。 |
| `src/bench_vfh.cpp` | bench wrapper 和 checksum 输出。 | `run_bench_compare`、board targets | production-public performance label。 |
| `script/generate_vfh_evidence_manifest.py` | repeated board summary 到 Evidence Doctor manifest 的转换。 | `evidence_doctor_repeated` | 证据角色、A/B boundary（对照边界）和 case metadata。 |
| `doc/phases/*` | phase plan/result、matrix。 | phase loop 恢复 | 阶段取舍和继续 / 停止决策。 |
| `doc/*.zh.md` | topic-local doc suite。 | reviewer / worker | 测试、bench、代码地图和 evaluation 主归属。 |
| `doc-rvv/features/vfh-RVV.zh.md` | production 长期主题文档。 | maintainer / reviewer | 当前 adopted production behavior 主归属。 |

## Production helper 对照

| production 符号 | 作用 | 测试 / bench 对应 |
| --- | --- | --- |
| `pcl::detail::VFHNormalAoSFloatLayout` | normal 字段 AoS float layout gate。 | production helper hit test，fallback matrix。 |
| `pcl::detail::computeVFHNormalCentroidRVV` | normal centroid RVV 规约。 | Phase 030 candidate；Phase 040-060 production asm。 |
| `pcl::detail::accumulateVFHSPFHRVV` | f1/f2/f3 pair math、bin index precompute 和标量 histogram increment。 | `production_vfh_compute_default`，Phase 060 asm。 |
| `pcl::detail::accumulateVFHViewpointRVV` | viewpoint alpha、bin index precompute 和标量 histogram increment。 | `production_vfh_compute_default`，Phase 060 asm。 |
| `pcl::detail::computeVFHSignatureRVV` | production RVV gate 和完整默认 descriptor 生成。 | `VFHProductionRVV.*`，`production_vfh_compute_default`。 |
| `VFHEstimation::computeFeature` | public dispatch / fallback。 | public reference tests 和 production bench。 |

## 结构审计

当前 topic 已采用 `src/`、`include/`、`include/impl/` 和 `script/` 结构，匹配 `.agents/config/defaults.yaml`
的 `artifact_layout` 与 `test_support` 方向。`include/impl/vfh_reference.hpp` 同时承载 fixtures、reference
和 test-only candidate，职责较多但仍低于本 topic 的硬拆分需求；production closeout 已通过独立 doc suite
把测试、bench、优化证据和代码地图拆开。若后续扩展点型或 histogram scatter 专项，建议再按 fixtures /
reference / candidate 拆分内部头，避免单文件继续变大。
