# organized_edge_detection 函数级评估

## 范围和目标源码

目标源码是 `features/include/pcl/features/impl/organized_edge_detection.hpp`。当前评估聚焦
`OrganizedEdgeBase<PointT, PointLT>::extractEdges()` 的 depth discontinuity（深度突变）路径和
`assignLabelIndices()` 的标签索引收集语义。RGB Canny、normal Canny 和 RGB+normal 组合入口只作为
后续 scope（范围）保留。

## 函数 / 函数族作用速览

| 函数 / 函数族 | 作用 | 输入 / 输出状态 | 与主流程关系 | RVV 判断 |
| --- | --- | --- | --- | --- |
| `OrganizedEdgeBase::compute()` | 初始化 labels 并调用 depth edge 提取，再按 edge type 收集索引。 | 输入 organized cloud；输出 `PointCloud<Label>` 和 `vector<PointIndices>`。 | public entry（公开入口）。 | Phase 010 已接入并采纳 depth production RVV path。 |
| `OrganizedEdgeBase::extractEdges()` | 遍历内部像素，按 8 邻域 depth 差、NaN boundary 和跨 invalid search 写 label bits。 | 读取 `z`，按 bit 写 `labels[idx].label`。 | depth label hot loop（热点循环）。 | RVV production helper 覆盖全有限邻域主路径，invalid neighbor 搜索逐 lane 回退标量。 |
| `assignLabelIndices()` | 线性扫描 labels，按 edge type push index。 | 读取 label bits，写每类 `PointIndices`。 | 输出顺序语义的一部分。 | Phase 000 保持标量，不改变顺序；若后续成为主成本再单独消融。 |
| `OrganizedEdgeFromRGB::extractEdges()` | 构造灰度图并调用 `Edge::detectEdgeCanny()`。 | 读取 RGB，写 `EDGELABEL_RGB_CANNY`。 | 派生入口。 | 未覆盖；可能另开 RGB Canny 诊断。 |
| `OrganizedEdgeFromNormals::extractEdges()` | 构造 normal x/y 图并调用 Canny。 | 读取 normals，写 `EDGELABEL_HIGH_CURVATURE`。 | 派生入口。 | 未覆盖；normal Canny 成本和 2D edge helper 需另评估。 |

## 函数级结论

最终结论是 `production-adopted`。Phase 000 test-helper diagnostic（测试专用诊断）先证明 depth label loop
值得进入有界 production probe（生产探针）；Phase 010 随后完成真实 production 接入，并用接入后的
`OrganizedEdgeBase<PointXYZ, Label>::compute()` 证据确认采纳条件成立。Phase 020 再补齐
`PointXYZI`、`PointXYZRGB`、`PointXYZRGBNormal` 的 production-public（真实公开入口）证据，说明当前同一
depth RVV helper 对这些已测 traits-gated 点型也保持正向。

采纳范围是 depth label path：`__RVV10__` 构建下，`PointT` 满足 `RVVXYZAoSFloatLayout` 且 `PointLT`
为 `pcl::Label` 时先尝试 RVV；已测生产证据覆盖 `PointXYZ`、`PointXYZI`、`PointXYZRGB` 和
`PointXYZRGBNormal`。其它自定义点型、其它 label 类型或非 RVV 构建自然回到标量 helper。RGB / normal
Canny 前处理和 `assignLabelIndices()` 仍保持原路径。

## 标量流程与 RVV 流程对照

| 阶段 | 标量路径 | Phase 000 RVV candidate | 证据边界 |
| --- | --- | --- | --- |
| label 初始化 | `compute()` resize labels 并置 0。 | test helper 调用前 `std::fill_n` 置 0。 | 同构初始化；production 容器未修改。 |
| 全有限邻域 | 每像素分配 8 个距离、`minmax_element` 找 dominant distance，再按阈值写 occluding / occluded。 | VL chunk（可变向量长度分块）同时加载中心和 8 邻域 z，向量化 finite mask、min/max、dominant 和 label 写回。 | 覆盖主路径；反汇编确认有 `vle32.v`、`vfsub.vv`、`vfmin.vv`、`vfmax.vv`、`vcpop.m`、`vse32.v`。 |
| invalid neighbor | 标量累计 invalid 邻域方向，沿平均方向搜索对应 finite depth；找不到则写 NaN boundary。 | 若 chunk 中存在 invalid lane，逐 lane 调同一 scalar pixel helper 回填。 | 正确性已覆盖；该 fallback 会降低 invalid-heavy case 的收益。 |
| label indices | 按线性 index 和 edge type bit 顺序 push。 | Phase 000 保持标量 `assignLabelIndices()` helper。 | 顺序等价测试已覆盖；未做收集性能优化。 |

## Traceability Map

| 符号 / 文件 | 层级 | 作用 | 调用者 / 上游入口 | 被调用者 / 下游消费者 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- | --- | --- |
| `OrganizedEdgeBase::extractEdges()` | production hot loop | depth label 原始生产语义。 | `OrganizedEdgeBase::compute()` 和派生类 `compute()`。 | labels、`assignLabelIndices()`。 | production boundary（生产边界）来源。 | `features/include/pcl/features/impl/organized_edge_detection.hpp` |
| `organizedEdgeDepthLabelsStandard()` | production Std helper | 原 depth label 标量主体。 | `extractEdges()` fallback。 | labels。 | fallback baseline（回退基线）。 | `features/include/pcl/features/impl/organized_edge_detection.hpp` |
| `organizedEdgeDepthLabelsRVV()` | production RVV helper | `__RVV10__` 下批量处理全有限 8 邻域，invalid lane 调标量 pixel helper。 | `extractEdges()` dispatch。 | labels、`assignLabelIndices()`。 | adopted production path（已采纳生产路径）。 | `features/include/pcl/features/impl/organized_edge_detection.hpp` |
| `computeDepthLabelsScalar()` | diagnostic reference（诊断参考链路） | 复刻 depth label 标量语义。 | topic gtest / bench。 | correctness 对拍和 Std build bench。 | correctness baseline（正确性基线）。 | `test-rvv/features/organized_edge_detection/include/impl/organized_edge_detection_depth_labels.hpp` |
| `computeDepthLabelsRVV()` | candidate helper | RVV 全有限邻域 fast path + invalid lane 标量 fallback。 | topic gtest / bench。 | board repeated summary、asm dump。 | diagnostic candidate。 | `test-rvv/features/organized_edge_detection/include/impl/organized_edge_detection_depth_labels.hpp` |
| `src/test_organized_edge_detection.cpp` | correctness target | 检查 depth step、NaN boundary search、edge type bit gate 和 label index 顺序。 | `run_test_compare`。 | QEMU logs。 | correctness gate（正确性验收）。 | `test-rvv/features/organized_edge_detection/src/test_organized_edge_detection.cpp` |
| `src/bench_organized_edge_detection.cpp` | bench wrapper | 构造 finite / tail / invalid neighbor case 并输出 checksum。 | board repeated target。 | manifest、Evidence Doctor。 | board diagnostic performance。 | `test-rvv/features/organized_edge_detection/src/bench_organized_edge_detection.cpp` |
| `src/bench_organized_edge_detection_production.cpp` | production bench wrapper | 调用真实 `OrganizedEdgeBase<PointT, Label>::compute()`，覆盖 `PointXYZ` 和 Phase 020 点型 case。 | production repeated target。 | manifest、Evidence Doctor、doc-rvv。 | production-public board performance。 | `test-rvv/features/organized_edge_detection/src/bench_organized_edge_detection_production.cpp` |
| `script/generate_organized_edge_detection_evidence_manifest.py` | analysis script | 将 repeated run 目录转为 summary 和 manifest。 | `record_board_evidence_state`。 | Evidence Doctor、registry。 | evidence manifest（证据清单）。 | `test-rvv/features/organized_edge_detection/script/generate_organized_edge_detection_evidence_manifest.py` |

## 测试计划和 bench 计划

| 测试 / target | 层级 | 作用 |
| --- | --- | --- |
| `run_test_compare` | correctness aggregate（正确性汇总入口） | Std / RVV 构建各跑 7 个 gtest，确认 test helper 和真实 production `compute()` 的 label bits / label index 顺序等价。 |
| `dump_bench_rvv` | asm attribution（反汇编归属） | dump RVV bench 二进制，确认 candidate 符号附近存在关键 RVV 指令。 |
| `check_production_rvv_asm` | production asm attribution | dump production bench 二进制，确认 `organizedEdgeDepthLabelsRVV` 生产 helper 有关键 RVV 指令。 |
| `run_board_organized_edge_detection_repeated` | board repeated diagnostic（板卡重复诊断） | 5-run 板卡 Std/RVV compare，生成 repeated summary、manifest、Evidence Doctor 和 registry。 |
| `run_board_organized_edge_detection_production_repeated` | board repeated production-public（板卡重复生产证据） | 5-run 真实 `compute()` Std/RVV compare，生成 production summary、manifest、Evidence Doctor 和 registry。 |
| `run_board_organized_edge_detection_point_type_repeated` | board repeated production-public point-type expansion（点型扩展生产证据） | 5-run 真实 `compute()` compare，只运行 Phase 020 point-type case-filter，生成 point-type summary、manifest、Evidence Doctor 和 registry。 |
| `evidence_status` | registry / freshness（证据登记 / 新鲜度） | 检查 summary、manifest 和 doctor 是否 fresh。 |

## 验证结果

| 证据 | 结果 | 边界 |
| --- | --- | --- |
| QEMU correctness | `run_test_compare`：Std / RVV 构建各 7/7 pass。 | QEMU 只证明正确性和日志形状，不证明真实性能。 |
| asm attribution | `build/asm/riscv/bench_organized_edge_detection_rvv.full.asm` 中 `computeDepthLabelsRVV` clone 附近有 `vle32.v`、`vfsub.vv`、`vfmin.vv`、`vfmax.vv`、`vcpop.m`、`vse32.v`。 | 归属到 test-only candidate，不是 production 符号。 |
| board performance | `log/board/repeated-summary.md`：finite 320x240 mean `4.446x`，finite 641x481 tail mean `4.617x`，NaN boundary 320x240 mean `3.075x`。 | 只支撑 diagnostic performance；invalid-heavy production 分布仍需真实入口验证。 |
| Evidence Doctor | `log/board/evidence_doctor.md`：Errors=0，Warnings=0，Suggestions=3。 | Suggestions 是环境字段缺失；不阻塞 diagnostic 结论。 |
| evidence registry | `log/evidence_registry.json` 记录 summary、manifest、doctor 为 fresh。 | raw repeated logs 默认不提交；summary-only 进入提交候选需 reviewer 确认。 |

## 生产接入判断

当前判断是 `production-adopted`。生产接入后的最终证据如下：

| 项 | 结果 | 证据路径 |
| --- | --- | --- |
| production patch scope | `organized_edge_detection.hpp` 新增 Std/RVV depth helpers，`extractEdges()` 做短分流；public API 不变。 | `features/include/pcl/features/impl/organized_edge_detection.hpp` |
| production direct tests | Std/RVV 构建各 7/7 pass，含真实 `compute()` 对拍和 Phase 020 typed production tests。 | `log/qemu/run_test_std.log`、`log/qemu/run_test_rvv.log` |
| production asm | `organizedEdgeDepthLabelsRVV` 有 `vle32.v`、`vfsub.vv`、`vfmin.vv`、`vfmax.vv`、`vse32.v`。 | `build/asm/riscv/bench_organized_edge_detection_production_rvv.full.asm` |
| production board bench | `prod_depth_finite_320x240` mean `6.194x`，`prod_depth_finite_641x481_tail` mean `5.521x`，`prod_depth_nan_boundary_320x240` mean `3.016x`，checksum match。 | `log/board/production-repeated-summary.md` |
| Evidence Doctor | Errors=0，Warnings=0，Suggestions=3。 | `log/board/production-evidence_doctor.md` |
| point-type expansion board bench | `PointXYZI` mean `5.327x`，`PointXYZRGB` mean `5.437x`，`PointXYZRGBNormal` mean `4.158x`，`PointXYZRGB` NaN boundary mean `2.822x`，checksum match。 | `log/board/point-type-repeated-summary.md` |
| point-type Evidence Doctor | Errors=0，Warnings=1，Suggestions=4；Warning 为 `PointXYZRGBNormal` 组内收益较低但仍 positive。 | `log/board/point-type-evidence_doctor.md` |
| evidence registry | fresh。 | `log/evidence_registry.json` |

Decision delta（决策变化）：Phase 000 的 diagnostic speedup 只作为候选价值信号；最终采纳依据改为 Phase 010
production direct board evidence。生产证据比诊断证据更强，且没有 checksum mismatch 或 Doctor Error。

## 诊断证据链

Phase 000 证明：在 test helper 边界，depth label 主循环可以用 RVV 批量处理并保持标签等价；在合成 organized grid 的三组板卡 case 中，RVV build 相对 Std build 稳定正向。它不能证明：production dispatch 已存在、派生 RGB / normal 入口已覆盖、泛型点类型安全、真实 workload 中 invalid neighbor 比例和 `assignLabelIndices()` 成本不会稀释收益。

## 正确性与高效性证据链

- correctness（正确性）：`run_test_compare` 中真实 production `compute()` 对拍通过；labels 和
  `assignLabelIndices()` 顺序与标量 reference 等价。Phase 020 后 Std/RVV 均为 7/7。
- path / asm（路径 / 反汇编）：`check_production_rvv_asm` 证明生产 helper 存在并包含关键 RVV 指令；test-only
  diagnostic asm 不再作为采纳依据。
- performance（性能）：性能结论只来自 production repeated board summary，不使用 QEMU timing。
- boundary（证据边界）：当前证据证明 `PointXYZ`、`PointXYZI`、`PointXYZRGB`、`PointXYZRGBNormal` + `pcl::Label`
  的 depth path；其它自定义点型和泛型 `PointLT` 不能写成已证明。
- risk（风险）：RGB / normal Canny、泛型 `PointLT`、未测自定义点型和 `assignLabelIndices()` RVV 化均保留为后续 scope expansion 或 follow-up。
