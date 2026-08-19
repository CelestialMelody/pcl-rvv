# NDT Test Support Code Map

本文帮助 reviewer 从文档定位到测试支撑代码、bench wrapper、script 和 output。所有 helper 都是 test-only diagnostic（测试专用诊断），不修改 production。

## 调用图

```text
src/test_ndt.cpp / src/bench_ndt.cpp
  -> include/ndt.h
     -> ndt_fixtures.hpp
     -> ndt_references.hpp
     -> ndt_candidates.hpp
        -> RVV intrinsics under __RVV10__
  -> script/generate_ndt_qemu_evidence_manifest.py
  -> script/generate_ndt_board_repeated_summary.py
  -> ../../script/evidence_doctor.py
  -> ../../script/evidence_registry.py
```

## 代码地图

| 符号 / 文件 | 层级 | 作用 | 证据角色 |
| --- | --- | --- | --- |
| `registration/include/pcl/registration/impl/ndt.hpp` | production source | 当前被评估源码；未修改 | production boundary |
| `computeDerivatives` | production hot path candidate | 遍历 input 点和 neighborhood，调用 point derivative 与 updateDerivatives | 当前源码事实 |
| `include/impl/ndt_fixtures.hpp` | fixture | 生成 deterministic staged sample | correctness / bench input |
| `include/impl/ndt_references.hpp` | diagnostic reference | 标量公式参考链路 | correctness baseline |
| `include/impl/ndt_candidates.hpp` | diagnostic candidate | RVV staged derivative accumulation；phase 020 含 topic-local double `exp` prototype | candidate under test |
| `src/test_ndt.cpp` | test wrapper | 4 个 gtest correctness gate | QEMU / board correctness |
| `src/bench_ndt.cpp` | bench wrapper | 输出 analyzer 可解析 bench log 和 checksum | QEMU smoke / board performance |
| `script/generate_ndt_qemu_evidence_manifest.py` | script | 生成 QEMU smoke manifest | Evidence Doctor input |
| `script/generate_ndt_board_repeated_summary.py` | script | 汇总 5 次 board A/B 并生成 manifest | Board summary / doctor input |

## 拆分审计

当前 topic 已采用 `src/`、`include/`、`include/impl/`、`script/`、`doc/` 布局；没有旧 `test_support/` 目录、legacy alias（兼容别名）或超长单文件 helper。当前拆分状态为 `adopted`。
