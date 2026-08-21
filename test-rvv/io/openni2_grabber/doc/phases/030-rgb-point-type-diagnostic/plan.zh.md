# Phase 030 Plan: rgb-point-type-diagnostic

## 阶段意图和边界

本阶段回答“是否可以进行泛型点型测试”。当前 adopted production behavior（已采纳生产行为）仍只覆盖 `convertToXYZPointCloud` 的具体 `PointXYZ` depth-only path；它不是模板入口，不能做泛型点型生产证明。

可测试的泛型面来自 `convertToXYZRGBPointCloud<PointT>` 的 RGB/RGBA 模板入口。本阶段只在 test-rvv production-shaped diagnostic（生产形态诊断）层补 `PointXYZRGB` 与 `PointXYZRGBA` 的同边界 correctness（正确性）覆盖，不修改 production patch，不声明 RGB/RGBA 可采纳。

| item | boundary |
| --- | --- |
| validated_scope | `PointXYZRGB` / `PointXYZRGBA`，`Scalar=float`，连续 synthetic depth/RGB buffer，同尺寸与整数 step mismatch。 |
| unvalidated_scope | 任意用户自定义点型、`PointXYZRGBNormal`、`PointXYZRGBL`、真实 OpenNI2 public entry、非整数 resize ratio。 |
| production scope | 不扩大；`io/src/openni2_grabber.cpp` 仍只保留 adopted `PointXYZ` depth-only helper。 |
| performance scope | 不跑板卡性能；Phase 000 已证明 RGB/RGBA diagnostic median 1.02x，当前不支持 production 接入。 |

## 实现和测试动作

| action | artifact | completion evidence |
| --- | --- | --- |
| 泛型 wrapper | `include/openni2_grabber.h` | `fillRGBOverlayCandidate` 可用于 `PointXYZRGB` 和 `PointXYZRGBA`。 |
| correctness test | `src/test_openni2_grabber.cpp` | `make run_test_compare` Std/RVV 通过，新增 `PointXYZRGB` / `PointXYZRGBA` bitwise 对拍。 |
| 文档同步 | phase result、matrix、evaluation、roadmap、handoff | 明确“可做泛型诊断测试，但不扩大 production adopted scope”。 |

## Continue / Stop Criteria

如果 correctness 失败，先修 test support；如果 correctness 通过，阶段关闭为 `diagnostic coverage adopted / no production expansion`。除非出现新的稳定 positive board evidence 或用户要求扩大 RGB/RGBA production probe，否则不继续接入 RGB/RGBA production。
