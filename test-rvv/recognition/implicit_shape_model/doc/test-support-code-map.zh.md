# Test Support Code Map

| 文件 | 职责 | 证据角色 | 说明 |
| --- | --- | --- | --- |
| `recognition/include/pcl/recognition/impl/implicit_shape_model.hpp` | production source | adopted production behavior | `findObjects()` 通过 `findNearestClusterIndex<FeatureSize>()` 分流 nearest cluster assignment；`__RVV10__` 下统一走 RVV helper，`153` 只是当前证据的代表性维度 |
| `include/ism.h` | aggregator header（聚合头） | test support | 给 test / bench 提供稳定 include 入口 |
| `include/impl/ism_diagnostics.hpp` | internal helper（内部 helper） | diagnostic | 保存 Std/RVV 局部公式候选和输入构造；不作为 production helper |
| `src/test_ism.cpp` | correctness source | correctness | gtest 对拍局部公式和 production helper |
| `src/bench_ism.cpp` | bench source | diagnostic / production direct bench | 输出 analyzer 可解析的计时和 checksum；包含 `public_find_objects_descriptor_assignment` |
| `script/generate_ism_evidence_manifest.py` | evidence script | Evidence Doctor input | 从 repeated board 日志生成 summary / manifest，并按 case metadata 写 evidence role |
| `Makefile` | host/QEMU harness | build/test/bench/evidence | 定义 correctness、upstream test、asm、board repeated、doctor 和 registry target |
| `board.mk` | board harness | board | 板卡侧运行参数 |
| `doc-rvv/recognition/implicit_shape_model-RVV.zh.md` | production topic doc | long-term maintenance | 记录当前 adopted production 行为、fallback 和证据链 |

## 布局审计

当前已采用 `src/`、`include/`、`include/impl/`、`script/` 布局；没有旧 `test_support/`
目录和 compatibility alias（兼容别名）。root-level 超长测试源文件不存在。

## 代码职责边界

- production helper 和 test-only diagnostic helper 分开维护。`findNearestClusterIndexRVV` 是真实生产
  RVV path，`assignDescriptorBatchCandidate` 等只用于诊断和 bench。
- public-entry bench 使用测试专用 `SyntheticIsmFeature` 构造稳定 descriptor，但调用的 production
  `findObjects()` 和 nearest-cluster helper 是真实生产源码路径。
- evidence script 的 case metadata 决定 `diagnostic`、`production_shaped_diagnostic` 或
  `production_direct`，避免把所有 summary 都写成局部诊断。
