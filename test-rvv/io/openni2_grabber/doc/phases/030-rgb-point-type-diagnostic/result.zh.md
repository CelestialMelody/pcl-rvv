# Phase 030 Result: rgb-point-type-diagnostic

## 结论

可以进行泛型点型测试，但边界必须写窄：当前 adopted production patch（已采纳生产补丁）仍只覆盖非模板 `convertToXYZPointCloud` 的具体 `PointXYZ` depth-only path；泛型测试只适用于 `convertToXYZRGBPointCloud<PointT>` 对应的 RGB/RGBA 模板诊断面。

本阶段已把 test-rvv helper 的 RGB overlay（颜色覆盖）和 depth-to-RGB-point（深度到彩色点型 xyz 写入）路径扩到 `PointXYZRGB` 与 `PointXYZRGBA`，并新增 `RGBTemplatePointTypesMatchScalarBitwise`。`make run_test_compare` 在 Std/RVV 两侧均为 8 个 gtest 通过。

## 执行结果

| action | status | evidence |
| --- | --- | --- |
| 泛型 wrapper | done | `include/openni2_grabber.h` 中 `fillRGBOverlayScalar/RVV` 模板化，新增 `PointXYZRGB` wrapper。 |
| correctness test | done | `src/test_openni2_grabber.cpp` 新增 `RGBTemplatePointTypesMatchScalarBitwise`，覆盖 `PointXYZRGB` / `PointXYZRGBA`。 |
| production scope check | done | 未修改 `io/src/openni2_grabber.cpp`；adopted production scope 仍为 depth-only `PointXYZ`。 |
| verification | done | `make run_test_compare`：Std/RVV 各 8 个 gtest 通过。 |
| QEMU bench smoke | done | `make run_bench_rvv BENCH_ARGS="--case-filter all --iterations 2 --warmup-iterations 1"` 可运行；只证明日志形状和 bench 构建。 |

## 证据边界

| question | result |
| --- | --- |
| 是否证明当前 production patch 泛型化 | 否。`convertToXYZPointCloud` 不是模板入口。 |
| 是否证明 RGB/RGBA 模板入口的点型语义可测 | 是。`PointXYZRGB` 与 `PointXYZRGBA` 在诊断 helper 上与标量 reference bitwise 对齐。 |
| 是否支持 RGB/RGBA production 接入 | 否。Phase 000 板卡 repeated 的 RGB overlay median 仅 1.02x，仍是 neutral / near-threshold。 |
| 是否需要板卡重跑 | 不需要。本阶段只补 correctness coverage（正确性覆盖）和 QEMU smoke（小型验证），没有新的 performance candidate（性能候选）。 |

## Continue / Stop Decision

`continue_stop_decision`: stop_no_worthwhile_same-topic_optimization。

当前可采纳生产优化已经是 depth-only `PointXYZ`。泛型点型诊断覆盖已补齐；RGB/RGBA、mismatch 和 IR 仍缺少稳定 positive 证据，不值得继续接 production。public-entry OpenNI2 test 需要 OpenNI2-enabled RISC-V 环境；legacy `openni_grabber.cpp` parity 属于独立 topic。
