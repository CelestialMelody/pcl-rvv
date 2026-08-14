# ICP transformCloud RVV 主题文档

## 当前状态

`registration/include/pcl/registration/impl/icp.hpp` 中
`IterativeClosestPoint::transformCloud` 已接入 RVV production path。当前 EvidenceDecision 为
`production_direct_positive`：QEMU / 板卡 correctness 通过，Milkv-Jupiter production direct
5-run repeated benchmark 稳定正向，反汇编归因指向 production `transformCloud` 符号。

本文是长期 production 说明，只记录 adopted 行为、dispatch / fallback、范围边界、证据链和维护风险。
测试工程细节见 `test-rvv/registration/icp/doc/` 下的 topic-local docs。

## 函数语义

`IterativeClosestPoint::transformCloud` 接收 input cloud、output cloud 和 4x4 transform。`computeTransformation`
在初始 guess、每轮迭代后更新 transformed input，以及最终输出阶段调用它。

上游语义保留如下：

- transform matrix 先 cast 到 `Eigen::Matrix4f`。
- XYZ 字段按 `x_idx_offset_ / y_idx_offset_ / z_idx_offset_` 运行期 offset 读取。
- XYZ 任一分量非有限时跳过整个点，不写 XYZ，也不处理 normal。
- XYZ 有限时执行 `transform * [x, y, z, 1]`，只写回 XYZ 字段。
- `source_has_normals_` 为 true 时，再按 normal offsets 读取 normal。
- normal 任一分量非有限时只跳过 normal 写回，已经写出的 XYZ 保持变换后状态。
- normal 有限时使用 transform 左上角 3x3 rotation 写回 normal。
- 函数允许 input 和 output 是同一对象。

RVV path 不执行 `output = input`；它只写标量路径会写的字段。调用者预置 output、resize 后 output 或 in-place
输出的责任仍在原调用边界。

## 当前采用的优化方式

### Dispatch 与 Fallback

production 入口结构：

```text
IterativeClosestPoint::transformCloud
  -> cast Matrix4 to Matrix4f
  -> if __RVV10__ and Scalar=float:
       tryTransformCloudRVV(...)
       if true return
  -> transformCloudStandard(...)
```

RVV path 只在以下条件全部满足时启用：

| 条件 | RVV 决策 | fallback 决策 |
| --- | --- | --- |
| build macro | `__RVV10__` 才编译 RVV helper。 | 非 RVV build 只编译 Std helper。 |
| `Scalar` | `Scalar=float` 才尝试 RVV。 | `Scalar=double` 等类型回退。 |
| input size | `input.size() >= 32`。 | 小规模回退，避免短数组噪声和额外 gate 成本。 |
| point layout | `kRVVXYZAoSPointCompatible` 或 `kRVVXYZNormalPointCompatible`。 | traits 不通过回退。 |
| runtime offsets | runtime `x/y/z` 和 optional normal offsets 必须等于 traits offsets。 | offset mismatch 回退。 |
| normals | `source_has_normals_ == false` 走 XYZ；true 走 XYZ+normal。 | normal layout 不兼容回退。 |

### Generic Layout Gate

RVV helper 模板化在 `PointSource` 上。它不只匹配 exact `PointXYZ` / `PointNormal`，而是使用 PCL RVV
point traits 判断 registered float AoS layout：

- `kRVVXYZAoSPointCompatible<PointSource>`：提供 float XYZ offsets。
- `kRVVXYZNormalPointCompatible<PointSource>`：提供 float XYZ + normal offsets。

运行期 offsets 必须与 traits offsets 一致，避免 `setInputSource()` 解析出的字段布局与编译期 layout 不一致。
当前 correctness 覆盖 `PointXYZI` 和 `PointXYZINormal`；performance repeated 代表点型仍是 `PointXYZ`
和 `PointNormal`。

### XYZ RVV Path

`transformCloudXYZRVV` 使用 strided AoS load 读取 x/y/z：

1. `vsetvl_e32m2` 选择 VL。
2. `vlsseg3e32` 按 point stride 读取 x/y/z。
3. `finiteMaskF32M2ICP` 分别检查 x/y/z 的 NaN / Inf。
4. 三个 mask 合并成 `keep`。
5. 使用 `vfmul` / `vfmacc` / `vfadd` 计算 tx/ty/tz。
6. 用 masked strided store 只写 `keep` lane 的 x/y/z。

该 path 对应 board cases：

- `icp transform-cloud xyz 64K`
- `icp transform-cloud xyz 256K`

### XYZ+Normal RVV Path

`transformCloudXYZNormalRVV` 先执行同样的 XYZ masked transform，再读取 normal_x/y/z：

1. 生成 `keep_xyz`，控制 XYZ 写回。
2. 读取 normal_x/y/z。
3. 生成 normal finite mask。
4. `keep_normal = keep_normal && keep_xyz`，保持“XYZ 非有限时整点跳过”的上游语义。
5. 使用 transform 左上角 3x3 rotation 计算 normal。
6. 用 masked strided store 写回 normal。

normal mask 不能替代 XYZ mask；normal 非有限时不能回滚已经写出的 XYZ。

该 path 对应 board cases：

- `icp transform-cloud xyz-normal 64K`
- `icp transform-cloud xyz-normal 256K`

### 代码路径与标量差异

| 维度 | 标量路径 | RVV 路径 | 保持方式 |
| --- | --- | --- | --- |
| 字段访问 | per-point `memcpy` + runtime offsets | traits offsets + strided load/store | runtime offset gate。 |
| 有限值判断 | `std::isfinite` | `finiteMaskF32M2ICP` | gtest 覆盖 NaN / Inf。 |
| XYZ 计算 | Eigen 4x4 multiply | FMA 展开 3 行 | `2e-5f` 误差预算。 |
| normal 计算 | Eigen 3x3 multiply | FMA 展开 3 行 | normal direct tests。 |
| 写回字段 | 只写 XYZ / optional normal | masked store 同字段 | generic point tests 检查其它字段保持。 |
| fallback | 原函数循环 | `transformCloudStandard` helper | helper 保留上游语义。 |

## 范围决策表

| 范围 | 决策 | 理由 |
| --- | --- | --- |
| `PointXYZ` / `Scalar=float` / compatible AoS | adopted | correctness、board repeated 和 asm attribution 均闭合。 |
| `PointNormal` / `Scalar=float` / compatible AoS | adopted | XYZ+normal correctness、board repeated 和 asm attribution 均闭合。 |
| `PointXYZI` compatible XYZ layout | adopted with representative performance | production direct correctness 覆盖；performance 由 `PointXYZ` 代表。 |
| `PointXYZINormal` compatible XYZ+normal layout | adopted with representative performance | production direct correctness 覆盖；performance 由 `PointNormal` 代表。 |
| `Scalar=double` | fallback | RVV helper 只覆盖 float data path；double test 保护标量 fallback。 |
| `input.size() < 32` | fallback | 避免小规模向量 path 噪声；small input test 保护。 |
| runtime offset mismatch | fallback | 防止编译期 traits 与运行期 field mapping 不一致。 |
| `IterativeClosestPointWithNormals` | not_applicable | 该类 override 调用 `pcl::transformPointCloudWithNormals`。 |
| indices / correspondences | not_applicable | `transformCloud` 只扫描已 materialized input cloud。 |
| ICP end-to-end speedup | not_claimed | 当前 bench 只计 `transformCloud` full-cloud microbench。 |

## Bench 与性能证据

性能结论只来自 board / target hardware。当前 board repeated summary：
`test-rvv/registration/icp/log/board/transform_cloud_repeated/summary.md`

运行上下文：

| key | value |
| --- | --- |
| device | `Milkv-Jupiter` |
| evidence role | `production_direct` |
| runs / iterations / warmup | `5 / 20 / 3` |
| governor / freq / temperature | `performance / 1600000 / 41000` |
| taskset | `not_pinned` |

Production direct board median：

| case | median speedup | min | max |
| --- | ---: | ---: | ---: |
| `icp transform-cloud xyz 64K` | 5.68x | 5.29x | 6.50x |
| `icp transform-cloud xyz 256K` | 5.30x | 5.17x | 5.32x |
| `icp transform-cloud xyz-normal 64K` | 3.76x | 3.62x | 3.97x |
| `icp transform-cloud xyz-normal 256K` | 3.93x | 3.85x | 4.07x |

Doctor warnings 均来自 `PointXYZ 64K`：long-tail / variance 和 group outlier。由于全部 run 均明显正向，
当前不扩大复跑预算，但保留 min/median/max，且不把该 case 收益外推到其它 case。

## 正确性与高效性证据链

| 证据 | 状态 | 说明 |
| --- | --- | --- |
| QEMU correctness | done | `test-rvv/registration/icp/log/qemu/run_test_std.log` 和 `test-rvv/registration/icp/log/qemu/run_test_rvv.log` 均为 12 tests passed。 |
| board correctness | done | `test-rvv/registration/icp/log/board/run_test.log` 为 12 tests passed。 |
| board performance | done | `test-rvv/registration/icp/log/board/transform_cloud_repeated/summary.md`，Milkv-Jupiter 5-run production direct。 |
| Evidence Doctor | done | `test-rvv/registration/icp/log/board/transform_cloud_repeated/evidence_doctor.md`：Errors=0，Warnings=2，Suggestions=0。 |
| manifest | done | `test-rvv/registration/icp/log/board/transform_cloud_repeated/evidence_manifest.json`：`evidence_role=production_direct`。 |
| asm attribution | done | `test-rvv/registration/icp/doc/asm-attribution.zh.md`：RVV 指令簇位于 production `transformCloud` 符号内。 |
| evidence registry | done | `test-rvv/registration/icp/log/evidence_registry.json` 记录当前 production direct evidence。 |
| doc-suite parity | done | `test-rvv/registration/icp/doc/phases/004-structure-parity-doc-suite/result.zh.md`。 |

## Fallback 矩阵

| fallback 场景 | 触发条件 | 验证 |
| --- | --- | --- |
| non-RVV build | 未定义 `__RVV10__` | QEMU Std 12 tests passed。 |
| unsupported Scalar | `Scalar != float` | `ProductionDirectScalarDoubleFallback`。 |
| small input | `input.size() < 32` | `SmallInputFallsBack` 和 `ProductionDirectSmallInputFallback`。 |
| runtime offset mismatch | runtime offsets 不等于 traits offsets | `MismatchedRuntimeOffsetsFallback`。 |
| incompatible point layout | PCL RVV traits 不通过 | 编译期不进入 RVV branch，走 Std helper。 |
| normal layout mismatch | `source_has_normals_` true 但 XYZ+normal traits/offset 不匹配 | Std helper。 |

## QEMU Bench Compare 策略

QEMU 只用于 correctness、build 和日志形状，不用于性能结论。公共 Makefile 已默认禁止 QEMU
`run_bench_compare`；只有为了历史/窄范围 smoke，且显式设置 `ALLOW_QEMU_BENCH_COMPARE=1`
并写明 `qemu_smoke_only`，才可运行。历史 QEMU bench 文件
`test-rvv/registration/icp/log/qemu/run_bench_std.log`、
`test-rvv/registration/icp/log/qemu/run_bench_rvv.log`、
`test-rvv/registration/icp/log/qemu/analyze_bench_compare.log`、
`test-rvv/registration/icp/log/qemu/evidence_manifest.json` 和
`test-rvv/registration/icp/log/qemu/evidence_doctor.md` 不进入性能排序或 EvidenceDecision。

## 遗留风险与后续条件

| 风险 / 后续 | 当前处理 |
| --- | --- |
| `PointXYZ 64K` board warning | 保留 min/median/max 和 Doctor warning；若 reviewer 要更稳证据，可追加 20-run board confirmation。 |
| generic 点型 performance | 当前只做 correctness；若某个下游以 `PointXYZI` / `PointXYZINormal` 为热点，可补 representative board cases。 |
| ICP end-to-end profile | 当前不声称端到端整体加速；若需要产品级收益，应新增 end-to-end ICP profile。 |
| `IterativeClosestPointWithNormals` | 另属 `pcl::transformPointCloudWithNormals`，需要独立 topic。 |
