# min_cut_segmentation 正确性测试

## 本文职责

本文解释 correctness（正确性）测试的输入、断言和证明边界。它不承担 benchmark（性能测试）统计，也不把 helper-level pass 写成 public `extract()` 语义通过。

## 测试文件分工

| 文件 | 职责 |
| --- | --- |
| `src/test_min_cut_segmentation.cpp` | gtest 入口、fixture、向量对拍断言 |
| `include/min_cut_segmentation.h` | topic-local 聚合头 |
| `include/impl/min_cut_segmentation_components.hpp` | Std reference（标量参考）、RVV candidate（候选实现）、buildGraph-shaped helper |
| `segmentation/include/pcl/segmentation/impl/min_cut_segmentation.hpp` | production 标量语义来源；本 topic 未修改 |

## 共同输入和断言

所有测试使用合成 dense `PointXYZ` cloud，xyz 为 `float`，布局为 AoS（结构数组）。`foreground` 使用确定性坐标生成，`indices` 覆盖完整输入。误差阈值按 RVV float path 与 production double formula 的差异设置：

| 对象 | 阈值 | 理由 |
| --- | ---: | --- |
| unary sink vector | `2e-6` | RVV 使用 float lane 计算和 `vfsqrt` |
| unary checksum | `1e-2` | 长向量 checksum 会累积 float rounding（舍入）差异 |
| binary weight vector | `2e-5` | RVV 使用 `expf_RVV_f32m2` finite-domain approximation（有限域近似） |
| binary checksum | `2e-3` | checksum 只用于同构路径对拍 |
| buildGraph capacity checksum | `2e-1` | 包含大量 unary / binary edge capacity 的累计误差预算 |

## TEST 字典

| TEST | 输入 | 被测路径 | 断言 | 证明范围 | 不能证明 |
| --- | --- | --- | --- | --- | --- |
| `UnaryPotentialMatchesScalar` | `4096` 点、`19` foreground | `computeUnaryPotentialsStd` vs `computeUnaryPotentialsRVV` | sink vector、sink checksum、source checksum | unary component formula 同构 | graph 写入和 public dispatch |
| `BinaryPotentialMatchesScalarWithinFloatExpBudget` | `4096` 点、2 条 synthetic edge / point | `computeBinaryPotentialsStd` vs `computeBinaryPotentialsRVV` | weight vector、weight checksum | binary distance + expf 近似在预算内 | double `std::exp` 严格替换 |
| `BuildGraphPotentialBatchMatchesScalarWithinFloatBudget` | `768` 点、`19` foreground、`14` neighbours | `computeBuildGraphPotentialBatchStd` vs `computeBuildGraphPotentialBatchRVV` | vertex / edge counts、unary / binary logical edge counts、capacity checksum | KNN + Boost graph 写入边界内的 helper 同构 | public `extract()`、max-flow 和泛型 `PointT` |

## 边界和随机样本策略

当前测试是确定性 synthetic fixture，不使用随机种子。空输入、非 dense 输入、indices subset、泛型点类型、NaN / Inf 和 fallback 不在当前授权证据范围内；这些边界不会影响 no-production closeout，因为当前没有生产接入。

## 验证命令

| 命令 | 作用 |
| --- | --- |
| `make -C test-rvv/segmentation/min_cut_segmentation run_test_compare` | Fresh correctness gate；Std / RVV 两侧均运行 |
| `make -C test-rvv/segmentation/min_cut_segmentation run_test_std` | 只验证非 RVV 宏构建 |
| `make -C test-rvv/segmentation/min_cut_segmentation run_test_rvv` | 只验证 RVV 宏构建 |

当前 closeout 前 fresh verification：`run_test_compare` 通过，Std / RVV 各 3 个 tests。
