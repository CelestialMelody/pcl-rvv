# PFH RVV 函数级评估

## 范围

目标源码是 `features/include/pcl/features/impl/pfh.hpp`，评估入口为
`pcl::PFHEstimation<PointInT, PointNT, PointOutT>::computePointPFHSignature` 以及上游
`computeFeature` public KSearch（公开 K 近邻搜索）路径。当前生产补丁只在 `__RVV10__` 构建中启用。

## 函数作用速览

| 函数 / 函数族 | 作用 | 输入 / 输出状态 | RVV 判断 |
| --- | --- | --- | --- |
| `computeFeature` | 对每个输入点搜索邻域并生成 `PFHSignature125`。 | `indices_`、search method、normals、output cloud。 | 只通过下游 helper 间接受益；邻域搜索保持标量。 |
| `computePointPFHSignature` | 对单个邻域的所有点对计算 PFH histogram。 | cloud、normals、neighbor indices、`nr_split`、`Eigen::VectorXf` histogram。 | O(k^2) pair feature 是当前 RVV 主路径。 |
| `computePairFeatures` | 计算两点 Darboux frame 特征 `f1/f2/f3/f4`。 | 两点 xyz 和 normal。 | 在 direct AoS helper 中批量化其主要数学链路。 |
| pair feature cache | `use_cache_ == true` 时缓存点对特征。 | `std::map` key/value 状态。 | 当前不接管，直接 fallback。 |

## 生产补丁范围

`features/include/pcl/features/impl/pfh.hpp` 新增
`pcl::detail::computePointPFHSignatureDirectAoSRVV`。公开入口在 histogram 初始化后、原标量 pair loop 前，
在 `!use_cache_` 时尝试该 helper；helper 返回 `false` 时继续原标量路径。

已采纳 exact 组合：

- `pcl::PointNormal -> pcl::PointNormal`
- `pcl::PointXYZ -> pcl::Normal`

共同 gate（门控）：

- `nr_split == 5`
- `indices.size() >= 4`
- source / normals size 可用 32-bit byte offset 表示
- source indices 非负且在 source / normals 范围内
- source cloud 点为 finite（有限值）
- source 侧满足 xyz AoS float layout；normal 侧满足 normal AoS float layout

## 实现方式审计

| 实现维度 | 当前状态 | 证据 | 边界 / 恢复条件 |
| --- | --- | --- | --- |
| dispatch / fallback | adopted | `run_test_compare` Std 3/3、RVV 6/6 pass | 非 RVV 构建和未覆盖 gate 回到标量。 |
| direct AoS gather | adopted | `dump_bench_rvv` 中 production helper 含 `vluxei32.v` | exact 点型组合已证明；泛型 traits 未证明。 |
| pair tuple math | adopted | helper asm 含 `vfmacc.vv`、`vfsqrt.v`、`vfdiv.vv` | histogram scatter 保持标量，避免 bin conflict。 |
| staged SoA family | rejected for current production family | Phase 030 诊断慢于 direct AoS | 仅在 direct AoS 被回滚或新点型无法维护时恢复。 |
| cache path | not_now | 无 profile 指向 cache path 是当前瓶颈 | 若 profile 显示 cache 热，再新建 phase。 |
| 泛型 point traits | deferred | exact `PointXYZ + Normal` 已覆盖常见组合 | 需要公共 normal AoS gate、更多点型测试和板卡证据。 |

## 测试和 bench 计划 / 结果

| target / case | 层级 | 作用 | 当前结果 |
| --- | --- | --- | --- |
| `run_test_compare` | correctness aggregate | 同时运行 Std 与 RVV gtest，覆盖 reference、candidate、production helper 和 fallback。 | Std 3/3、RVV 6/6 pass。 |
| `DirectAoSHelperComputesPointNormalProductionHistogram` | production direct correctness | exact `PointNormal -> PointNormal` helper 与标量路径对拍。 | pass。 |
| `DirectAoSHelperComputesPointXYZNormalProductionHistogram` | production direct correctness | exact `PointXYZ + Normal` helper 与标量路径对拍。 | pass。 |
| `DirectAoSHelperFallsBackForPointXYZNormalUnsupportedGates` | fallback | 验证小邻域与非默认 bins 不命中 RVV。 | pass。 |
| `component_pfh_signature` | production detail / cross-check | 单邻域 component 入口，重复 128 次。 | Phase 040 mean `2.004x`；Phase 060 cross-check mean `1.964x`。 |
| `public_pfh_k` | production public | `PFHEstimation::compute` KSearch 公开路径。 | Phase 040 mean `1.926x`；Phase 060 cross-check mean `1.902x`。 |
| `component_pfh_signature_pointxyz_normal` | production detail | `PointXYZ + Normal` 单邻域 component 入口。 | Phase 060 mean `1.922x`。 |
| `public_pfh_pointxyz_normal_k` | production public | `PFHEstimation<PointXYZ, Normal>::compute` KSearch 公开路径。 | Phase 060 mean `1.854x`。 |

QEMU 只作为 correctness、构建和路径证据；性能结论只来自板卡 repeated benchmark。

## Fallback 矩阵

| 条件 | 行为 | 证据 |
| --- | --- | --- |
| 非 RVV 构建 | 不编译 RVV helper。 | Std tests 3/3 pass。 |
| 点型不是 exact `PointNormal -> PointNormal` 或 exact `PointXYZ -> Normal` | helper compile-time 返回 `false`。 | production gate；未覆盖组合保持标量。 |
| `use_cache_ == true` | 公开入口不尝试 RVV。 | source dispatch gate；测试 expected 侧用 cache 避免误命中 RVV。 |
| `nr_split != 5` 或邻域小于 4 | helper 返回 `false`。 | RVV fallback gtest。 |
| 负 index、越界或 normals 尺寸不足 | helper 返回 `false`。 | source gate；不改变既有标量异常边界。 |
| 32-bit byte offset 不可表达 | helper 返回 `false`。 | `rvvMaxU32ByteOffsetElements` gate。 |

## EvidenceDecision

| production scope | board evidence | Evidence Doctor | decision |
| --- | --- | --- | --- |
| exact `PointNormal -> PointNormal` | Phase 040 component mean `2.004x`，public mean `1.926x` | `0E/0W/8S` | `user_confirmed_adopted_production` |
| exact `PointXYZ -> Normal` | Phase 060 component mean `1.922x`，public mean `1.854x` | `0E/0W/12S` | `adopted_production` |

Phase 060 的 suggestions 仅是环境 metadata / binary hash 建议，不阻塞稳定 positive bucket。当前不把任一 exact
点型组合的结果外推到泛型点类型、cache path、OMP path、非默认 bins 或其它目标硬件。
