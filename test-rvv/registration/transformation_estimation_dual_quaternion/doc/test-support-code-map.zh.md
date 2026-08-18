# 测试支撑代码地图

## 本文职责

本文解释 `tedq` 测试支撑代码的结构、职责和拆分审计。当前 topic 使用 `src/`、`include/`
和 `include/impl/` 布局，没有旧 `test_support/` 目录。

## 总调用图

```text
src/test_tedq.cpp / src/bench_tedq.cpp
  -> include/tedq.h
    -> include/impl/tedq_adapters.hpp
      -> deterministic fixtures / source / target index / correspondence staging adapters
    -> include/impl/tedq_candidates.hpp
      -> scalar reference / test-only RVV candidates / checksums
  -> production header
      -> retained ordered/source-indexed/dual-indexed RVV helpers
      -> scalar correspondence public overload
```

## Internal helper 职责

| 文件 / 符号 | 职责 | 证据角色 |
| --- | --- | --- |
| `include/tedq.h` | 稳定聚合入口 | reviewer 定位入口 |
| `makePointXYZCloud`、`makePointXYZICloud`、`makePointXYZRGBCloud` | deterministic corpus 构造 | correctness / bench 输入 |
| `makeSourceIndices`、`makeTargetIndices`、`makeCorrespondences` | row-source 样本构造 | source / dual / correspondence 语义测试 |
| `estimatePublic*DualQuaternion` | 调用 production public overload | public scalar / production direct 对照 |
| `accumulateDualQuaternionStd` | 复刻 production C1/C2 累加 | same-chain reference |
| `estimateDualQuaternion*Candidate` | test-only RVV / diagnostic wrappers | diagnostic correctness / board evidence |
| `matrixChecksum`、`accumulationChecksum` | bench fingerprint | smoke / component evidence |
| production detail path-hit tests | 直接调用 header 内 retained RVV helper | production dispatch gate 证据 |

## 拆分审计

| shape | present | paths | decision | next action |
| --- | --- | --- | --- | --- |
| `src/` source | yes | `src/test_tedq.cpp`、`src/bench_tedq.cpp` | `adopted` | keep |
| aggregator header | yes | `include/tedq.h` | `adopted` | keep |
| `include/impl` internal helpers | yes | `tedq_adapters.hpp`、`tedq_candidates.hpp` | `adopted` | keep |
| old `test_support/` directory | no | not_present | `not_applicable with evidence` | none |
| topic-local scripts | yes | `script/generate_tedq_*.py` | `adopted` | keep |
| evidence registry | yes | `log/evidence_registry.json` | `adopted` | refresh before commit |

## Production 与 Test Support 边界

当前 production patch 只保留 ordered/source-indexed/dual-indexed 三类 RVV helper。
test support 中的 correspondence direct index stream、segment-load、locality 和 point-type layout
仍是诊断候选；它们不能证明当前 production dispatch，因为 correspondence production RVV 已移除。
