# SampleConsensusModelPlane RVV production 文档

## 当前生产状态

`SampleConsensusModelPlane<PointT>` 的 `selectWithinDistance`、`countWithinDistance` 和
`getDistancesToModel` 已采纳 RVV production（生产 RVV）路径。当前真实生产行为是：

- 三条公开入口在 `__RVV10__`、registered single-float xyz AoS（已注册单精度 xyz 结构数组）
  layout、32-bit byte offset（32 位字节偏移）规模门控成立时进入 RVV。除 `PointXYZ` 板卡性能外，
  `PointXYZI`、`PointXYZRGB`、`PointXYZRGBA` 和 `PointXYZINormal` 已有代表性 correctness（正确性）
  证据。
- `selectWithinDistanceRVV` 和 `countWithinDistanceRVV` 在 identity indices（恒等索引）VL chunk
  下使用 strided load（跨步加载），其它 chunk 回退 indexed gather（按索引离散加载）。
- `getDistancesToModelRVV` 保持 gather-only；Phase 010 尝试过 identity 分支，但未采纳。

## 函数语义

平面模型系数为 `(a,b,c,d)`，每个点的距离为 `abs(a*x + b*y + c*z + d)`。三个入口共享同一距离核：

| public entry | 职责 | 输出 |
| --- | --- | --- |
| `selectWithinDistance` | 距离小于阈值时保序写入点索引和距离。 | `Indices inliers`、`error_sqr_dists_`。 |
| `countWithinDistance` | 统计距离小于阈值的点数。 | `std::size_t`。 |
| `getDistancesToModel` | 为每个 index 写出距离。 | `std::vector<double> distances`。 |

## 当前采用的优化方式

| 维度 | 当前状态 | 采用或暂缓原因 | 证据 | 边界 / 下一步 |
| --- | --- | --- | --- | --- |
| indexed xyz gather + shared distance kernel | adopted | direct indexed `indices_` 是公开入口真实 row source；RVV 避免逐点 Eigen 临时对象。 | Phase 000 correctness、asm、5-run board；Phase 020 代表点型 correctness | 覆盖 `PointXYZ` board；`PointXYZI`、`PointXYZRGB/RGBA`、`PointXYZINormal` correctness。 |
| select `vcompress` writeback | adopted | inliers 和距离需要保序紧凑写回，`vcompress` 能把有效 lane（向量通道）直接压缩。 | select Phase 000 median 3.1965x，Phase 010 identity median 3.3896x | 不覆盖其它输出型模型。 |
| count mask + `vcpop` | adopted | 计数入口只需要掩码真值数量，`vcpop` 可避免标量逐点累加。 | count Phase 000 median 1.6678x，Phase 010 identity median 2.1969x | 不改变阈值比较语义。 |
| identity-index strided load for select/count | adopted_narrow | 默认整云路径常见 `indices[i] == i`，strided load 降低 gather 成本；shuffled fallback 未退化。 | Phase 010 identity/shuffled 5-run board + asm | 只用于 select/count。 |
| identity-index strided load for getDistances | rejected | 同类分支没有稳定收益，早期 mixed gate 曾让 shuffled case 回落。 | Phase 010 result 和 Evidence Doctor | 当前 helper 显式强制 gather；重开需 RVV-vs-RVV detail A/B。 |

## VL chunk 流程

RVV helper 每次用 `vsetvl` 选择当前 VL chunk。通用流程是加载 `indices_`，根据点型 layout 把 index
转换为点结构体内 `x/y/z` 字段的字节偏移，计算 `abs(a*x+b*y+c*z+d)`。

`selectWithinDistanceRVV` 构造 `distance < threshold` mask，使用 `vcompress` 保序压缩 index 和距离，
再拓宽为 double 写入 `error_sqr_dists_`。`countWithinDistanceRVV` 对同一 mask 执行 `vcpop`。
这两个入口在 chunk 内索引全部等于点云下标时，改用 `strided_load3_f32m2` 从结构数组连续跨步读取
`x/y/z`。

`getDistancesToModelRVV` 仍按 `indices_` gather 读取 `x/y/z`，随后把 float 距离拓宽为 double 并
密集写入 `distances[i]`。

## 覆盖范围与 fallback

| 条件 | 当前行为 |
| --- | --- |
| 非 RVV 构建 | 走 Standard / SSE / AVX 既有路径。 |
| `RVVXYZAoSFloatLayout<PointT>::value == false` | 公开入口不进入 RVV，保持 Standard fallback。 |
| 点云规模超过 32-bit byte offset 可表达范围 | Standard fallback。 |
| shuffled 或非 identity `indices_` | select/count 在该 chunk 使用 gather；getDistances 始终使用 gather。 |
| NaN/Inf 或非 dense 数据 | 不新增特殊语义；保持原入口的模型校验和点访问语义。 |
| `Scalar=double` / 非 `Eigen::VectorXf` 系数 | 不在当前采纳范围内。 |

## Traceability Map

| 符号 / 文件 | 层级 | 作用 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- |
| `selectWithinDistance` / `countWithinDistance` / `getDistancesToModel` | production public entry | RVV dispatch 或 Standard fallback。 | production boundary | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_plane.hpp` |
| `sacModelPlaneRVVLoadXYZ` | production detail helper | select/count 的 identity strided load 和 gather fallback；getDistances 强制 gather。 | load strategy boundary | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_plane.hpp` |
| `src/test_sac_model_plane.cpp` | test | 7 个 gtest 覆盖公开入口、helper、代表点型、identity 和显式空 `indices_`。 | correctness gate | `test-rvv/sample_consensus/sac_model_plane/src/test_sac_model_plane.cpp` |
| `src/bench_sac_model_plane.cpp` | bench | public Std/RVV timing，支持 identity/shuffled。 | board performance | `test-rvv/sample_consensus/sac_model_plane/src/bench_sac_model_plane.cpp` |
| `script/generate_board_evidence_manifest.py` | evidence script | 生成 Evidence Doctor manifest。 | metadata boundary | `test-rvv/sample_consensus/sac_model_plane/script/generate_board_evidence_manifest.py` |
| `doc/phases/010-identity-index-strided-load/result.zh.md` | phase result | Phase 010 证据解释和采纳 / 拒绝决定。 | EvidenceDecision | `test-rvv/sample_consensus/sac_model_plane/doc/phases/010-identity-index-strided-load/result.zh.md` |

## 正确性与高效性证据链

| 层级 | 证据 | 结论 |
| --- | --- | --- |
| correctness（正确性） | `make -C test-rvv/sample_consensus/sac_model_plane run_test_compare`；Std/RVV 各 7 个 gtest 通过。 | public entry 与 Standard path 对齐，identity、显式空 `indices_` 和代表性 AoS 点型 correctness 已覆盖。 |
| board correctness（板卡正确性） | `run_board_base_plane_public_tests`；RVV gtest 7/7 通过。 | 真实板卡可执行新增代表点型和显式空 `indices_` 测试；Makefile clock skew 是环境 warning。 |
| asm（反汇编） | `make -B -C test-rvv/sample_consensus/sac_model_plane dump_bench_rvv`。 | select/count 有 `vlsseg3e32` 和 gather fallback；getDistances 只有 `vluxei32` gather。 |
| board performance（板卡性能） | Phase 000 和 Phase 010 5-run repeated board。 | 三入口基础 RVV positive；Phase 010 只为 select/count 提供额外 identity 收益。 |
| Evidence Doctor | Phase 000 Errors=0/Warnings=0；Phase 010 adopted entries 无 Error/Warning。 | metadata Suggestions 不阻塞；getDistances long-tail Warning 支持不采纳 identity 分支。 |
| boundary（边界） | evaluation、optimization matrix 和 phase result。 | 不外推到其它 SAC 模型、其它 row source、`Scalar=double` 或未验证点型性能。 |

## 数值算例

对点 `(x,y,z)=(2,3,0.02)` 和平面系数 `(0,0,1,0)`，距离为
`abs(0*2 + 0*3 + 1*0.02 + 0)=0.02`。当阈值为 `0.05` 时，该点进入
`selectWithinDistance` 的输出，`countWithinDistance` 加一，`getDistancesToModel` 写出 `0.02`。
RVV 路径在一个 VL chunk 内对多个点执行同一公式；identity select/count 只改变 x/y/z 的加载方式，
不改变公式、mask 或输出顺序。

## 后续方向

当前没有新的未阻塞 RVV performance candidate。更多点型的 dedicated board performance、evidence
registry 和环境 metadata 硬化可以作为提交 / 归档增强；`getDistancesToModel` identity 分支不作为默认
下一步，只有新设计能解释并消除现有长尾和 shuffled 退化风险时再重开。
