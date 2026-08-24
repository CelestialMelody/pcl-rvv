# 020 PI1 production-integration-plan

## 阶段意图和边界

本阶段是 PI1 production integration plan（生产接入计划），只冻结生产补丁候选范围和证据计划，不修改 production（生产源码）。进入 PI2 production patch 需要用户明确授权。

## 候选范围

| 维度 | PI1 冻结范围 |
| --- | --- |
| production 文件 | `features/include/pcl/features/impl/don.hpp` |
| public entry | `DifferenceOfNormalsEstimation<PointInT, PointNT, PointOutT>::computeFeature(PointCloudOut&)` |
| 首个 production probe 点型 | `PointNT=pcl::Normal`，`PointOutT=pcl::Normal`，`Scalar=float` |
| row source | ordered normal cloud，与 `input_->size()` 一一对应 |
| RVV 片段 | small / large normal 三字段差、finite mask 置零、curvature sqrt、normal / curvature 写回 |
| 不接入范围 | 非 `pcl::Normal` 的 `PointNT` / `PointOutT`、`Scalar=double`、其它 normal-like 自定义点型、前置 normal estimation |

## Fallback 与 dispatch 计划

首个 production patch 应保持公开 API 不变。建议形态：

1. 把当前标量主体抽成 `computeFeatureStd` 等价 helper，保留原有语义。
2. 在 `#if defined(__RVV10__)` 下提供 `computeFeatureRVV` 或窄 helper。
3. `computeFeature()` 只做短路：若 exact `pcl::Normal` gate、输出大小和运行规模满足条件，则走 RVV；否则自然落回 Std。
4. 非 RVV build 不出现常驻“只返回 false”的 RVV stub。

fallback matrix：

| 条件 | 行为 |
| --- | --- |
| 非 RVV build | Std fallback |
| `PointNT` 或 `PointOutT` 不是 exact `pcl::Normal` | Std fallback；不得写成泛型已覆盖 |
| 输入规模过小或 output size 与 `input_->size()` 不一致 | Std fallback 或保持当前 production 语义 |
| small / large normal cloud 尺寸错误 | 仍由 `initCompute()` 失败路径处理 |
| normal 差产生 NaN / Inf | RVV path 必须和 Std 一样写零 normal 和零 curvature |

## 泛型点类型策略

当前 PI1 有意收窄为 exact `pcl::Normal`。理由是 production 模板入口同时读取 `PointNT` normal 字段并写 `PointOutT` normal / curvature 字段；要扩大到 normal-like 泛型，需要证明：

- `PointNT` 有 `normal_x/y/z` 三个单个 float 字段，offset 和 AoS stride 满足 RVV load；
- `PointOutT` 有 `normal_x/y/z` 和 `curvature` 单个 float 字段，offset 和 AoS stride 满足 RVV store；
- `PointNT` 与 `PointOutT` 可以是不同点型时，两侧分别 gate；
- fallback tests 覆盖非 exact 点型继续走标量。

这些属于后续 point-type expansion phase，不在本阶段扩大。

## Production direct 测试计划

| 证据 | target / 产物 | 完成判据 |
| --- | --- | --- |
| production direct correctness | 在 `src/test_don.cpp` 增加真实 `DifferenceOfNormalsEstimation` 命中 RVV 的测试或新增 production-direct test | QEMU Std/RVV 和 board RVV correctness 通过 |
| fallback correctness | 非 exact `PointOutT` 或非 RVV build fallback case | fallback 输出与 Std 一致 |
| asm attribution | `dump_bench_rvv` 或 production direct bench asm | RVV 指令归属到 production helper / inlined public entry 范围 |
| board production bench | production direct bench target | repeated board summary，无 Evidence Doctor Error |
| Evidence Doctor / registry | production run-labelled summary / manifest / doctor / registry | fresh registry，doctor finding 已解释 |

## 暂停条件

- 生产 patch 需要修改 public API 或 `don.h` 公共类声明。
- exact `pcl::Normal` gate 无法隔离，导致其它模板实例误命中 RVV。
- production direct correctness、fallback、asm 或 board evidence 任一项失败。
- 用户未授权进入 PI2 production patch。

## 当前停止边界

本阶段计划可以作为下一 worker 的恢复入口。由于当前 prompt 未明确授权修改 production，默认 `next_worker_action` 是：向用户报告 PI1 已冻结，等待是否进入 production integration loop（PI2-PI5）。
