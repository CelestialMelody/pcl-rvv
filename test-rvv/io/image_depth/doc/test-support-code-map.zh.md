# image_depth test support code map

## 本文职责

本文帮助 reviewer 定位 test-rvv 代码、bench wrapper、script 和 output。它不承担性能结论；性能结论见 `benchmark-and-evidence.zh.md`。

## 总调用关系

```text
io/src/image_depth.cpp public entry
  -> production Std / RVV helper

src/test_image_depth.cpp
  -> include/image_depth.h scalar reference
  -> include/image_depth.h RVV candidate
  -> VectorFrameWrapper
  -> pcl::io::DepthImage public entry
  -> gtest assertions

src/bench_image_depth.cpp
  -> include/image_depth.h candidate entry
  -> pcl::io::DepthImage public entry for prod_* labels
  -> bench log
  -> script/generate_image_depth_repeated_summary.py
  -> script/generate_image_depth_evidence_manifest.py
  -> ../../script/evidence_doctor.py
  -> ../../script/evidence_registry.py
```

## 代码职责表

| 路径 / 符号 | 职责 | 证据角色 |
| --- | --- | --- |
| `include/image_depth.h` | 聚合头和测试专用 helper；包含 scalar reference、selector、RVV candidate | production-shaped diagnostic |
| `fillDepthMetersScalar` / `fillDisparityScalar` | 标量参考链路，复刻 production 内层语义 | correctness oracle |
| `selectDepthMetersCandidatePath` / `selectDisparityCandidatePath` | RVV / scalar path-selection gate | fallback / dispatch diagnostic |
| `fillDepthMetersContiguousRVV` / `fillDisparityContiguousRVV` | contiguous RVV candidate | candidate implementation |
| `fillDepthMetersDownsampleRVV` / `fillDisparityDownsampleRVV` | downsample `vlse16` RVV candidate | candidate implementation |
| `io/src/image_depth.cpp` production helpers | Std fallback、contiguous RVV、downsample RVV 和 public entry dispatch | production-public candidate |
| `src/test_image_depth.cpp` | diagnostic 和 production-public correctness gtest | correctness evidence |
| `src/bench_image_depth.cpp` | diagnostic bench harness、production-public `prod_*` bench 和 case registry | bench evidence |
| `script/generate_image_depth_repeated_summary.py` | repeated compare logs -> summary | summary producer |
| `script/generate_image_depth_evidence_manifest.py` | summary / logs -> Evidence Doctor manifest | manifest wrapper |
| `Makefile` | build, QEMU, board, doctor, registry targets | harness |
| `board.mk` | board remote binary names and shared board runner include | board harness |

## Fixtures 与输入构造

测试和 bench 各自构造 deterministic synthetic pixels（确定性合成像素）。输入覆盖 valid 值、`0`、`2047` 和 `65535`；bench 覆盖 full-size tight row、padded row 和 downsample。

## 拆分审计

当前 topic 已使用配置解析出的 `src/`、`include/` 和 topic-local `script/`。`include/image_depth.h` 同时承载 reference、selector 和 candidate helper，但文件规模仍低于硬拆分门槛；若进入 PI2 production direct 后继续增加 production-entry adapters 或 raw/OpenNI helpers，应创建 `include/impl/` 并按 reference、candidate、assertion、bench harness 职责拆分。

## production 与 test support 边界

`include/image_depth.h` 中所有 helper 都是 test-only（仅测试使用）。phase 050 的真正 production patch 位于 `io/src/image_depth.cpp`，公开 API 不变；test-rvv 通过 `PCL_RVV_IMAGE_DEPTH_TEST_HOOK` 观察 path hit，该 hook 只在 topic harness 编译 production cpp 时打开。
