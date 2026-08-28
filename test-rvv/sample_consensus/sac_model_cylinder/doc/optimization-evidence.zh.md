# sac_model_cylinder 优化证据索引

## 本文职责

本文把已尝试、已采纳、暂缓和不适用的 optimization candidate（优化候选）映射到代码、测试、bench、反汇编、
板卡和 EvidenceDecision（证据决策）。它不替代 `doc/optimization-roadmap.zh.md`：roadmap 记录搜索空间和恢复队列。

## 当前结论摘要

| 状态 | candidate family | 说明 |
| --- | --- | --- |
| adopted | production count/select indexed-gather + radial-norm + normal-angle | Phase 020 production direct correctness、asm、5-run board 和 Doctor 闭合。 |
| adopted | production getDistances full-RVV dense double store | Phase 030 接入后由 fresh production direct board 继续验证，基础点型 median `6.9318x`，Doctor 0/0/0。 |
| adopted | representative point-type / layout expansion | Phase 040 证明 `PointXYZI + Normal`、`PointXYZRGB + Normal` 和 `PointXYZ + PointNormal` 三组新增代表点型均为 positive。 |
| historical | Phase 000 diagnostic count/select candidate | 作为实现族来源保留，不再作为生产性能结论。 |
| not_applicable now | optimizeModelCoefficients staging | 当前距离三入口已闭合；该入口由 Eigen / LM helper 主导，需 profile 显示为热点后再启动。 |

## 优化方式总表

| candidate family | 代码路径 | correctness | bench / board | asm evidence | Evidence Doctor | decision | 边界 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| production count/select | `sample_consensus/.../impl/sac_model_cylinder.hpp` | `run_test_compare` Std/RVV 11/11 通过 | production manifest：基础 count `5.3124x`，select `4.6445x`；三组新增代表点型也全部 positive | `check_production_asm`：count 306 条 RVV，select 338 条 RVV | production Doctor 0/0/0 | adopted | direct indexed 四组代表点型证据。 |
| production getDistances dense writeback | `sample_consensus/.../impl/sac_model_cylinder.hpp` | public-vs-Standard 小样本、bench-shaped 4096 点和 typed correctness 逐项 `1e-5` 通过 | production manifest：基础 getDistances `6.9318x`；typed medians `6.7851x` / `7.3186x` / `6.2453x` | `getDistancesToModelRVVCylinder` 298 条 RVV，含 `vfsqrt.v` / `vfwcvt.f.f.v` / `vse64.v` | production Doctor 0/0/0 | adopted | dense output vector，checksum 只保护规模，数值由 gtest 保护。 |
| representative point-type expansion | `src/test_sac_model_cylinder.cpp`、`src/bench_sac_model_cylinder.cpp`、manifest wrapper | 三个 typed gtest 覆盖 source stride 和 normal offset | `PointXYZI + Normal`、`PointXYZRGB + Normal`、`PointXYZ + PointNormal` 三入口 5-run 全部 positive | 归属到同三个 production helper | production Doctor 0/0/0 | adopted for representative point types | 不外推到自定义点型全集。 |
| Phase 000 diagnostic | `include/impl/sac_model_cylinder_diagnostic.hpp` | 历史 diagnostic 3/3 通过 | diagnostic count `7.22x`，select `6.32x` | diagnostic helper 74 / 82 条 RVV | historical Doctor 0/1/0 | historical / superseded | 不作为当前 production 性能结论。 |
| identity-index strided load | not implemented | not_run | not_run | not_run | not_run | deferred | 只有真实 workload 和同边界 RVV-vs-RVV A/B 显示 gather 成本成为瓶颈时再做。 |

## 标量路径与 RVV 路径差异

三个 public entry 都先检查 `isModelValid`，再由 `__RVV10__` 构建尝试 RVV helper；若 gate 不满足，公开入口自然回到
Standard helper（标量 helper）。非 RVV 构建不实例化 RVV helper。

RVV 路径在 VL chunk（可变向量长度分块）中用 indexed gather 读取 point xyz 和 normal xyz，计算轴向投影后的
radial distance（径向距离）和 acute normal angle（锐角法线夹角）。`countWithinDistance` 使用 mask popcount
（掩码计数），`selectWithinDistance` 使用 `vcompress.vm` 保序压缩 index 并用 `vfwcvt + vse64` 写 double error，
`getDistancesToModel` 对每个 index 写 dense double distance。

## Fallback 与暂缓范围

| 范围 | 当前状态 | 原因 / 证据 | 下一步条件 |
| --- | --- | --- | --- |
| 非 RVV 构建 | adopted fallback | 编译时无 RVV helper，Std 构建 gtest 11/11。 | 无。 |
| layout / normal traits 不满足 | adopted fallback | `kCylinderRVVLayoutCompatible` 和 normal AoS gate 控制。 | 新点型 phase 补 traits correctness。 |
| `pcl::index_t` 不是 signed 32-bit | adopted fallback | RVV path 依赖 32-bit index load / compressed store。 | 若 PCL index type 改变，重新评估。 |
| cloud / normal 超过 32-bit byte offset | adopted fallback | `rvvMaxU32ByteOffsetElements` gate。 | 有真实超大输入需求时另测。 |
| normal cloud 小于 input cloud | adopted fallback for count/select | gtest 覆盖标量懒读取边界。 | getDistances 不适用该 fallback case。 |
| 自定义点型全集 / `Scalar=double` | deferred | 当前只批准四组代表点型和 `Eigen::VectorXf`。 | 有真实调用点、profile 或用户点名后新建窄 phase。 |
| `optimizeModelCoefficients` | not_applicable now | 当前三入口 distance kernel 已是主要已授权范围；优化阶段由 Eigen solver 主导。 | profile 显示它成为主成本。 |
