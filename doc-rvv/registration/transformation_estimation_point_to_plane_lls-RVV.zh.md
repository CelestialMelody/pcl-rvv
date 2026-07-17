# registration/transformation_estimation_point_to_plane_lls RVV 诊断说明

## 收尾摘要

`TransformationEstimationPointToPlaneLLS::estimateRigidTransformation` 本轮完成 bench（性能测试）诊断闭环，不修改 production（生产源码）。RVV candidate（RVV 候选链路）覆盖 PointNormal 全云和 correspondences（对应关系）路径的字段读取、finite mask（有限值掩码）和逐点 `a/b/c/d` 公式 staging（暂存阶段），随后回到 scalar tail（标量尾段）按原顺序累加 normal-equation（法方程）。

QEMU correctness（QEMU 正确性验证，不代表真实性能）、bench 日志解析和反汇编路径均已通过；board evidence（板卡证据）显示 full-cloud 只有 `0.96x` 到 `0.99x`，correspondences 只有 `0.52x` 到 `0.58x`。因此本主题结论是：保留 test-rvv 诊断，不进入生产分流。

## 1. 函数入口作用

该入口用于 point-to-plane ICP（点到平面迭代最近点）中的 LLS（线性最小二乘）变换估计。公开重载支持全云、source indices、source/target indices 和 correspondences。所有公开重载最终构造 `ConstCloudIterator`，进入同一个 protected 实现：

```text
source / target iterator
  -> finite check
  -> per-pair a,b,c,d formula
  -> accumulate ATA / ATb
  -> Eigen inverse solve
  -> construct 4x4 transform
```

本轮没有修改这些公开 API（应用程序接口）和上游模板声明。

## 2. 标量路径与诊断边界

标量实现对每个有效 correspondence 执行：

```text
a = nz * sy - ny * sz
b = nx * sz - nz * sx
c = ny * sx - nx * sy
d = nx * dx + ny * dy + nz * dz - nx * sx - ny * sy - nz * sz
```

随后累加 `ATA += row^T * row` 和 `ATb += row * d`。这段是可批处理公式，但最终 6x6 / 6x1 累加是浮点规约。直接用 RVV reduction（向量规约）会改变累加树；本轮选择更保守的 production-shaped diagnostic（生产形态诊断）：RVV 只做字段读取、finite mask 和公式 staging，再按 lane 顺序回到标量累加。

## 3. 覆盖范围与 fallback

当前 diagnostic（诊断代码）覆盖：

- `pcl::PointNormal` 全云连续路径；
- `pcl::Correspondences` 乱序和重复索引路径；
- 小规模 `n < 64` fallback（回退路径）；
- NaN/Inf invalid lane（无效 lane）过滤；
- `vlmax_e32m2 <= 64` 固定 buffer gate（固定缓冲验收条件）；
- source/target 点数对应 byte offset（字节偏移）可放入 32-bit offset（32 位字节偏移）。

当前不覆盖：

- production 泛型点类型 traits；
- `Scalar=double` 生产分流；
- 真正改变累加树的 vector reduction；
- weighted LLS 和 symmetric LLS。

## 4. 详细设计

| staging 名称 | 结构 / helper | 字段来源 | 下一段消费者 / tail | gate |
| --- | --- | --- | --- | --- |
| full-cloud formula staging | `accumulate_candidate_full` | source `x/y/z`，target `x/y/z` 和 `normal_x/y/z` | `accumulate_staged_rows` 标量 normal-equation tail | `n >= 64`、`vlmax_e32m2 <= 64`、32-bit byte offset |
| correspondences formula staging | `accumulate_candidate_correspondences` | correspondences 展开的 source/target index，再 gather 字段 | `accumulate_staged_rows` 标量 normal-equation tail | 同上，且有效 index 数量 `>= 64` |

RVV helper（RVV 辅助函数）使用 `vlse32.v` 处理连续 PointNormal 全云，使用 `vluxei32.v` 处理 correspondences gather（对应关系索引读取）。finite mask 用 `abs(value) <= float_max`，NaN 会比较失败，Inf 会超过最大有限值。有效 lane 通过 `vcompress.vm` 保序压缩到固定 64 lane buffer（固定长度临时缓冲），再由标量 tail 逐 lane 累加。

## 5. 数值算例与 VL chunk 说明

假设一个 VL chunk（可变向量长度分块）有 4 个 lane，输入里第 2 个 target normal 是 NaN：

```text
lane:      0      1      2      3
finite:   true   true   false  true
```

RVV 公式 staging 会先为 4 个 lane 计算候选 `a/b/c/d`，但 keep mask（保留掩码）只有 `1,1,0,1`。`vcompress` 后 buffer 顺序变成：

```text
buffer[0] <- lane 0
buffer[1] <- lane 1
buffer[2] <- lane 3
```

标量 tail 只按 `0,1,3` 的顺序调用累加逻辑。这与原标量循环遇到 lane 2 时 `continue` 的可见顺序一致。当前设计没有把 21 个 `ATA` 上三角项和 6 个 `ATb` 项做向量规约，因此避免了规约树变化；代价是压缩后仍有较重的标量累加成本，板卡结果也证实这部分成本没有被摊薄。

一个单点公式例子：

```text
source = (1, 2, 3)
target = (1.1, 1.9, 3.2)
normal = (0, 0, 1)
a = 1*2 - 0*3 = 2
b = 0*3 - 1*1 = -1
c = 0*1 - 0*2 = 0
d = 0*1.1 + 0*1.9 + 1*3.2 - 0*1 - 0*2 - 1*3 = 0.2
```

该 lane 对 `ATb` 的贡献是 `[0.4, -0.2, 0, 0, 0, 0.2]`，对 `ATA` 的贡献进入上三角矩阵。测试里 full-cloud 和 correspondences 都通过这种 staging 与标量 reference path（参考链路）对拍。

## 6. Bench case 说明

| case | 入口 | 规模 | 是否命中 RVV | 证明点 |
| --- | --- | --- | --- | --- |
| full-cloud 64K | `estimate_candidate_full` | 65536 | RVV 构建命中 | 连续 PointNormal staging 的基础收益。 |
| correspondences 64K | `estimate_candidate_correspondences` | 65536 输入点的 subset | RVV 构建命中 | gather staging 是否值得。 |
| full-cloud 256K | `estimate_candidate_full` | 262144 | RVV 构建命中 | 放大规模后能否摊薄压缩和标量 tail 成本。 |
| correspondences 256K | `estimate_candidate_correspondences` | 262144 输入点的 subset | RVV 构建命中 | 大规模 gather 是否仍退化。 |

## 7. 测试、QEMU、反汇编和板卡证据

- QEMU test：`make -C test-rvv/registration/transformation_estimation_point_to_plane_lls run_test_compare`，std/RVV 各 5 个测试通过。
- QEMU bench：`make -C test-rvv/registration/transformation_estimation_point_to_plane_lls run_bench_compare`，日志可解析，checksum 对齐；QEMU 不作为性能结论。
- 反汇编：`make -C test-rvv/registration/transformation_estimation_point_to_plane_lls dump_bench_rvv`，命中 `vlse32.v`、`vluxei32.v`、`vfmul.vv`、`vfadd.vv`、`vfsub.vv`、`vcpop.m`、`vcompress.vm`。
- 板卡：`make -C test-rvv/registration/transformation_estimation_point_to_plane_lls board_smoke`，专项测试通过，bench compare 显示 full-cloud `0.96x` 到 `0.99x`，correspondences `0.52x` 到 `0.58x`。

证据日志保留在 `test-rvv/registration/transformation_estimation_point_to_plane_lls/output/`、`build/asm/` 和 `log/`，不作为默认提交内容。

## 8. 生产接入评估

不接 production。当前方案的板卡性能没有成立，且 correspondences gather 明显变慢。若未来要重新评估，优先方向不是把当前 staging 直接搬入生产，而是单独研究 vector reduction 是否能在可接受误差内减少 scalar tail 成本；这需要新的数值语义测试、反汇编检查和板卡收益证据。

## 9. 后续方向

weighted LLS 可复用本主题的公式拆解经验，但不能默认复用性能结论；权重会改变 normal 缩放和数值范围。symmetric LLS 使用不同公式和 `rankUpdate` 结构，也应单独评估。当前 registration 队列可以继续转到 `transformation_estimation_point_to_plane_lls_weighted`，但应把本主题作为“保守 staging 无收益”的反例输入。
