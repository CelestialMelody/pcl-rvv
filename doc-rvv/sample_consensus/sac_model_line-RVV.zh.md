# SampleConsensusModelLine RVV 生产实现

## 当前生产状态

`SampleConsensusModelLine<PointT>` 当前已采纳三条 production RVV（生产 RVV）路径：

- `countWithinDistance` 使用 indexed xyz gather（按索引离散加载坐标）、直线距离平方公式、阈值 mask（掩码）和 `vcpop` 统计内点数量。
- `selectWithinDistance` 复用同一距离平方公式，并用 `vcompress`（向量压缩）保序写回 inlier index（内点索引）和平方误差；平方误差随后用 `vfwcvt`（float 到 double 拓宽转换）和 `vse64`（64 位向量写回）写入 `error_sqr_dists_`。
- `getDistancesToModel` 在 RVV chunk（可变向量长度分块）内计算 `vfsqrt`（RVV 向量平方根），再用 `vfwcvt`（float 到 double 拓宽转换）和 `vse64`（64 位向量写回）按原顺序写回 dense double distance output（连续 double 距离输出）。

当前采纳范围是 `PointXYZ` 风格 float xyz AoS（结构数组）点布局、direct indexed `indices_` 行来源、signed 32-bit `pcl::index_t`、可用 32-bit byte offset（字节偏移）表达的点云规模和 RVV 构建。其它点型、非 float xyz layout、非 RVV 构建或 offset gate 不满足时回到 Standard helper（标量 helper）。

## 函数语义和标量路径

line 模型使用六个 `Eigen::VectorXf` 系数：线上一点 `(px, py, pz)` 和方向 `(dx, dy, dz)`。三条公开入口都遍历 `indices_` 指向的输入点，先把方向归一化，再计算点到直线的叉乘距离：

```text
v = point - line_point
cross = v x normalized_line_direction
sqr_distance = cross.x^2 + cross.y^2 + cross.z^2
```

| public entry | 输出语义 | 当前 production 状态 |
| --- | --- | --- |
| `countWithinDistance` | 判断 `sqr_distance < threshold^2`，只返回内点数量。 | RVV adopted |
| `selectWithinDistance` | 判断同一阈值，按 `indices_` 顺序写 `inliers`，并写 `sqr_distance` 到 `error_sqr_dists_`。 | RVV adopted |
| `getDistancesToModel` | 为每个 index 输出 `sqrt(sqr_distance)`，输出顺序与 `indices_` 一致。 | RVV adopted |

`countWithinDistanceStandard`、`selectWithinDistanceStandard` 和 `getDistancesToModelStandard` 保存原标量主体，用于 fallback（回退路径）和 Std/RVV 对拍。

## 当前采用的优化方式

| 维度 | 当前状态 | 采用原因 | 证据 | 边界 / 下一步 |
| --- | --- | --- | --- | --- |
| `countWithinDistance` indexed gather + mask popcount | adopted | count 入口没有输出容器写回，RVV 可批量完成 xyz gather、叉乘平方距离和阈值判断，最后用 `vcpop` 规约 mask。 | Phase 060 production public board median `4.8068x`，Evidence Doctor 0/0/0。 | 只覆盖当前 direct indexed `PointXYZ` 风格 layout。 |
| `selectWithinDistance` indexed gather + `vcompress` + `vfwcvt` + `vse64` | adopted | select 需要保持 inlier 顺序，`vcompress` 保留 chunk 内 lane 顺序；Phase 060 把压缩后的平方误差直接拓宽成 double 并向量写回，去掉 scratch 和标量 lane loop。 | Phase 060 production public board median `3.2460x`，Evidence Doctor 0/0/0。 | 更多点型和 identity-index fast path 未采纳。 |
| `getDistancesToModel` indexed gather + `vfsqrt` + `vfwcvt` + `vse64` | adopted | Phase 020 证明“RVV 平方距离 + 标量 sqrt”退化；Phase 040 把 sqrt 移入 RVV chunk 后生产正向；Phase 050 再去掉 scratch 和标量 lane double store 后继续提升。 | Phase 060 production public board median `4.0460x`；Phase 050 getDistances-specific baseline median `4.2237x`。Evidence Doctor 0/0/0。 | dense double store 已由 RVV direct store 完成；更多点型和 identity-index fast path 未采纳。 |
| `getDistances-sqr-rvv-scalar-sqrt-store` | rejected | 该形状每点仍执行标量 sqrt 和 dense store，5-run board 全部低于标量。 | Phase 020 diagnostic median `0.9340x`，Evidence Doctor Errors=1。 | 只拒绝该实现族，不拒绝 `getDistancesToModel` RVV。 |
| identity-index strided load | rejected with evidence | Phase 070 做了 line 同边界 RVV-vs-RVV strict A/B。identity 输入下 count/getDistances 只有弱正向，select median `0.9967x` 且 4/5 退化；shuffled 控制组也不稳。 | identity doctor Errors=1 / Warnings=1 / Suggestions=2；shuffled doctor Errors=3 / Warnings=0 / Suggestions=2。 | 已回退到 Phase 060 gather-only load family；除非先冻结新的只覆盖 count/getDistances 的 production boundary，否则不重开。 |

## VL Chunk 流程

三个 RVV helper 的前半段相同：

1. `vsetvl` 选择当前 VL chunk。
2. 从 `indices_` 加载一段 32-bit index，并转成 `index * sizeof(PointT)` 的 byte offset。
3. 通过 `pcl::rvv_load::indexed_load3_f32m2` 按当前 `PointT` 的 xyz offset 读取坐标。
4. 广播 line point 和归一化方向，计算三维叉乘分量。
5. 用 FMA（融合乘加）累加 `cross_x^2 + cross_y^2 + cross_z^2`。

后半段按入口分流：

| helper | RVV 后半段 | 标量保留边界 |
| --- | --- | --- |
| `countWithinDistanceRVV` | `vmflt` 生成阈值 mask，`vcpop` 统计命中 lane。 | 只保留循环控制和总数累加。 |
| `selectWithinDistanceRVV` | `vcompress` 压缩命中的 index 和平方距离，连续写入预分配输出。 | 命中 lane 的 float 平方距离用 `vfwcvt + vse64` 直接写入 `error_sqr_dists_`；不再有逐 lane 标量写回。 |
| `getDistancesToModelRVV` | `vfsqrt` 在 RVV chunk 内得到 float distance，随后 `vfwcvt` 拓宽成 double，并用 `vse64` 直接写入 `distances`。 | 只保留循环控制；不再有 dense output 的逐 lane 标量写回。 |

`vcompress` 保留 chunk 内 lane（向量通道）顺序，chunk 按 `indices_` 顺序推进，因此 `selectWithinDistanceRVV` 的输出顺序和标量循环一致。

## 覆盖范围与 Fallback

| 条件 | 当前行为 |
| --- | --- |
| 非 `__RVV10__` 构建 | 只编译并调用 Standard helper。 |
| `pcl::rvv::RVVXYZAoSFloatLayout<PointT>::value == false` | public entry 的 `if constexpr` 不实例化 RVV helper，回到 Standard helper。 |
| `sizeof(pcl::index_t) != sizeof(std::int32_t)` 或非 signed index | 不进入 RVV helper，避免压缩写回 index 时改变语义。 |
| `input_->size() > pcl::rvv::rvvMaxU32ByteOffsetElements<PointT>()` | RVV helper 内部回到 Standard helper，避免 32-bit byte offset 溢出。 |
| model coefficients 无效 | 保持原公开入口副作用：count 返回 0，select 和 getDistances 不改写输出。 |
| `PointXYZI`、RGB/RGBA、normal 或自定义点型 | 未作为当前 production performance 采纳范围；满足 traits gate 时可能实例化，但 dedicated correctness / board 仍需后续 phase。 |
| `Scalar=double`、其它 row source 或完整 RANSAC 流程 | 当前不覆盖，不从本 topic 外推性能结论。 |

## Traceability Map

| 符号 / 文件 | 层级 | 作用 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- |
| `countWithinDistance` | production public entry | 做模型有效性检查并按 RVV gate 分流。 | production boundary | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_line.hpp` |
| `selectWithinDistance` | production public entry | 做模型有效性检查并按 RVV gate 分流。 | production boundary | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_line.hpp` |
| `getDistancesToModel` | production public entry | 做模型有效性检查并按 RVV gate 分流。 | production boundary | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_line.hpp` |
| `countWithinDistanceStandard` / `selectWithinDistanceStandard` / `getDistancesToModelStandard` | production Std helper | 保存原标量 fallback 语义。 | fallback coverage | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_line.hpp` |
| `countWithinDistanceRVV` / `selectWithinDistanceRVV` / `getDistancesToModelRVV` | production RVV helper | 执行 indexed xyz gather、直线距离公式、count/select/getDistances 后处理。 | production direct / asm attribution | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_line.hpp` |
| `src/test_sac_model_line.cpp` | correctness tests | 对拍 public entry 和测试专用 diagnostic candidate。 | QEMU correctness | `test-rvv/sample_consensus/sac_model_line/src/test_sac_model_line.cpp` |
| `src/bench_sac_model_line.cpp` | bench wrapper | 计时 public count/select/getDistances 和历史 diagnostic candidate。 | board performance input | `test-rvv/sample_consensus/sac_model_line/src/bench_sac_model_line.cpp` |
| `generate_line_board_evidence_manifest.py` | analysis script | 把 repeated board logs 转成 Evidence Doctor manifest。 | evidence summary | `test-rvv/sample_consensus/sac_model_line/script/generate_line_board_evidence_manifest.py` |
| Phase 040 manifest / doctor | evidence output summary | 保存接入后的 public production board baseline 数据。 | historical production baseline | `test-rvv/sample_consensus/sac_model_line/doc/phases/040-line-production-integration/` |
| Phase 050 manifest / doctor | evidence output summary | 保存 `getDistancesToModelRVV` direct double store 后的 public production board 数据。 | historical production baseline | `test-rvv/sample_consensus/sac_model_line/doc/phases/050-line-get-distances-vse64-store/` |
| Phase 060 manifest / doctor | evidence output summary | 保存 `selectWithinDistanceRVV` compressed double store 后的当前 public production board 数据。 | production evidence | `test-rvv/sample_consensus/sac_model_line/doc/phases/060-line-select-vse64-compressed-store/` |
| Phase 070 identity / shuffled manifest / doctor | evidence output summary | 保存 identity-index strided load 与 gather-only RVV 的同边界 A/B 拒绝证据。 | production-detail family selection evidence | `test-rvv/sample_consensus/sac_model_line/doc/phases/070-line-identity-index-strided-load/` |
| evaluation | topic-local decision audit | 保存诊断到生产接入的取舍和未覆盖范围。 | decision trace | `test-rvv/sample_consensus/sac_model_line/doc/sac_model_line-evaluation.zh.md` |

## 数值算例

设线上一点为 `(1.0, -2.0, 0.5)`，方向为 `(2.0, 1.0, -0.5)`。方向归一化后约为 `(0.8729, 0.4364, -0.2182)`。若输入点相对线上一点的偏移只沿垂直方向 `n=(0.2, -0.4, -0.8)`，且偏移长度约为 `0.1`，则：

```text
v = point - line_point
cross = v x normalized_line_direction
sqr_distance = |cross|^2 ~= 0.01
distance ~= 0.1
```

当阈值为 `0.10` 时，`countWithinDistance` 和 `selectWithinDistance` 使用严格 `< threshold^2` 判断；非常接近阈值的点必须由 correctness test 保护，避免 RVV float 公式把边界方向改写。`getDistancesToModel` 输出的是 `sqrt(sqr_distance)`，因此当前采纳的是带 RVV `vfsqrt`、`vfwcvt` 和 `vse64` 的实现族。

## Bench 与证据

当前 production 数据来自接入后的板卡 repeated 结果：

```text
SSH_AUTH_SOCK=<ssh-agent-socket> make -C test-rvv/sample_consensus/sac_model_line collect_repeated_board_production_evidence
make -C test-rvv/sample_consensus/sac_model_line record_repeated_board_production_evidence_state
SSH_AUTH_SOCK=<ssh-agent-socket> make -C test-rvv/sample_consensus/sac_model_line collect_repeated_board_production_vse64_evidence
make -C test-rvv/sample_consensus/sac_model_line record_repeated_board_production_vse64_evidence_state
SSH_AUTH_SOCK=<ssh-agent-socket> make -C test-rvv/sample_consensus/sac_model_line collect_repeated_board_production_select_vse64_evidence
make -C test-rvv/sample_consensus/sac_model_line record_repeated_board_production_select_vse64_evidence_state
```

| 证据 | 结果 | 说明 |
| --- | --- | --- |
| QEMU correctness | Phase 070 回退后 Std/RVV 各 9 个 gtest 通过。 | QEMU 只证明 correctness 和路径，不证明性能。 |
| production `countWithinDistance` board | median `4.8068x`，min/max `4.6009x / 4.8607x`。 | 支撑当前 adopted count RVV；Phase 060 未改 count。 |
| production `selectWithinDistance` board | median `3.2460x`，min/max `2.9755x / 3.3620x`。 | 支撑当前 adopted select RVV；RVV avg `0.772475 ms`，比 Phase 050 scratch + 标量 lane store baseline 的 `0.791395 ms` 小幅更快。 |
| production `getDistancesToModel` board | median `4.0460x`，min/max `4.0334x / 4.2406x`。 | 支撑当前 adopted getDistances RVV；Phase 060 未改 getDistances，Phase 050 getDistances-specific baseline RVV avg 为 `0.548416 ms`。 |
| production asm | 三个 RVV helper 均有预期 RVV 指令归属。 | `make check_production_asm`。 |
| Evidence Doctor | Errors=0，Warnings=0，Suggestions=0。 | `production-repeated-evidence-doctor.md`。 |

Evidence paths:

- `test-rvv/sample_consensus/sac_model_line/doc/phases/050-line-get-distances-vse64-store/production-repeated-evidence-manifest.json`
- `test-rvv/sample_consensus/sac_model_line/doc/phases/050-line-get-distances-vse64-store/production-repeated-evidence-doctor.md`
- `test-rvv/sample_consensus/sac_model_line/doc/phases/050-line-get-distances-vse64-store/production-repeated-evidence-doctor.json`
- `test-rvv/sample_consensus/sac_model_line/doc/phases/060-line-select-vse64-compressed-store/production-repeated-evidence-manifest.json`
- `test-rvv/sample_consensus/sac_model_line/doc/phases/060-line-select-vse64-compressed-store/production-repeated-evidence-doctor.md`
- `test-rvv/sample_consensus/sac_model_line/doc/phases/060-line-select-vse64-compressed-store/production-repeated-evidence-doctor.json`

## 正确性与高效性证据链

- correctness（正确性）：`run_test_compare` 在 Std/RVV 两侧均通过 9 个 gtest，覆盖乱序 direct indices、identity indices、bench-scale 输入、select 输出顺序、`error_sqr_dists_` 和 getDistances dense output；Phase 060 覆盖 select compressed error direct double store。
- path / asm（路径和反汇编）：`check_production_asm` 在 `countWithinDistanceRVV`、`selectWithinDistanceRVV` 和 `getDistancesToModelRVV` 中找到预期 RVV 指令，说明接入后 helper 不是死代码。
- performance（性能）：性能结论只来自 Milkv-Jupiter 板卡 5-run repeated board。QEMU timing 不作为性能证据。
- boundary（证据边界）：EvidenceDecision 只覆盖 direct indexed `indices_`、`PointXYZ` 风格 float xyz AoS、signed 32-bit index、u32 byte offset gate 和当前板卡。
- risk（风险）：更多点型、其它 layout、`Scalar=double`、完整 RANSAC 调用频率和其它硬件未由本次生产证据关闭；identity-index strided load 已由 Phase 070 同边界 A/B 拒绝。

## Production Closeout

| 文件 | 改动 | 回滚边界 |
| --- | --- | --- |
| `sample_consensus/include/pcl/sample_consensus/sac_model_line.h` | 新增 protected Standard/RVV helper 声明；public API 不变。 | 删除 helper 声明，并把公开入口恢复为原内联标量主体。 |
| `sample_consensus/include/pcl/sample_consensus/impl/sac_model_line.hpp` | 抽出三条 Standard helper，新增三条 RVV helper，公开入口增加 RVV gate 和 Standard fallback。 | 移除 RVV helper 和 dispatch，保留或内联 Standard helper 均可恢复原行为。 |
| `test-rvv/sample_consensus/sac_model_line/**` | 新增 correctness、bench、board、manifest、Evidence Doctor、registry 和 topic-local 文档。 | 测试资产可保留为诊断和回归证据。 |

## 后续方向

当前 adopted production boundary 内，两个明确的标量写回热点已经完成向量写回替换；identity-index strided load 也已由 Phase 070 同边界 A/B 拒绝。仍可单独开 phase 的方向包括：

- `point-type-expansion`：为 `PointXYZI`、RGB/RGBA、normal 或自定义点型补 dedicated correctness、asm、board repeated 和 Evidence Doctor。
- `getDistances` store 形态消融：如果后续出现更好的 float-to-double store 或批量转换方式，可重开 dense output 后处理优化。
- `identity-index-strided-load` 重开条件：只有出现更便宜的 identity 检测或入口拆分策略，并先冻结“只对 count/getDistances 生效、不影响 select”的新 production boundary，才值得重新评估。

这些方向都不改变当前三入口 `PointXYZ + direct indexed indices_` 生产采纳结论。
