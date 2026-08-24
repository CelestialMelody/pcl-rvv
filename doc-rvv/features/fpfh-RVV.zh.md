# FPFH RVV

## 当前状态

`features/include/pcl/features/impl/fpfh.hpp` 当前采用一条有界 RVV production path（生产路径）：
`FPFHEstimation::weightPointSPFHSignature` 在 `__RVV10__` 构建下先尝试
`pcl::detail::weightFPFHSignature33RVV`，用于默认 11+11+11 bins 的 FPFHSignature33（快速点特征直方图
33 维描述子）加权合成。gate（验收条件）不满足时继续执行原标量主体。

这份文档只描述已采用的 weighted FPFH 生产行为。Phase 000/001 的诊断候选、candidate 搜索空间和下一步计划
主归属在 `test-rvv/features/fpfh/doc/**`。

## 采纳口径与数据来源

本 topic 的生产采纳口径是：只有 production integration（生产接入）之后的板卡证据显示收益，才把当前补丁写成
adopted production behavior（已采用生产行为）。接入前的 diagnostic evidence（诊断证据）只能说明某个候选值得
进入 production probe（生产探针），不能作为最终采纳依据。

因此，本文件中的性能数据只采用 `features/include/pcl/features/impl/fpfh.hpp` 已接入 RVV helper 之后的
Milkv-Jupiter repeated board（重复板卡测试）结果：

- `production_detail` 主证据是 `component_weighted_spfh_33`，平均 `4.804x`，`0/5` degradation。
- `production_public` 公开入口证据是 `public_fpfh_k`，平均 `1.262x`，`0/5` degradation。
- Phase 001 的 `candidate_weighted_spfh_dense_rows` 平均 `1.858x` 只保留为 historical diagnostic（历史诊断）对照，
  不参与本文件的生产采纳判断。

这个口径对应 Phase 002 result 中的 EvidenceDecision（证据决策），证据路径是
`test-rvv/features/fpfh/log/board/pi1-production-weighted/repeated`。

## 函数语义和标量路径

FPFH 公开主路径是 `FPFHEstimation::computeFeature(PointCloudOut&)`：

1. 先调用 `computeSPFHSignatures` 为输入点及其邻居构建 SPFH（Simple Point Feature Histogram，简化点特征直方图）矩阵。
2. 对每个输出点重新搜索邻域，并通过 `spfh_hist_lookup` 把 surface index 映射到 SPFH matrix row。
3. 调用 `weightPointSPFHSignature`，按邻居距离 `1 / dist` 把三段 11-bin SPFH histogram 加权累加成 33-bin FPFH。
4. 每段 11 bins 分别归一化到 100，再复制到 `PointOutT::histogram`。

RVV 当前只接管第 3-4 步中的 weighted FPFH 合成。KdTree search、SPFH 构建、`computePairFeatures`、
SPFH lookup、输出复制和 OMP 路径仍保持既有实现。

## 当前采用的优化方式

| 维度 | 当前状态 | 采用或暂缓原因 | 证据 | 边界 / 下一步 |
| --- | --- | --- | --- | --- |
| dispatch / fallback（分流 / 回退） | `__RVV10__` 下短路尝试 `weightFPFHSignature33RVV`；失败则执行原标量主体。 | production diff 小，公开 API 不变，fallback 清楚。 | QEMU Std/RVV 6/6 pass。 | 非 RVV 构建自然保持标量。 |
| 输入布局 | 三个 Eigen column-major SPFH matrix，每段 11 bins；输出为 contiguous 33-bin vector。 | 原 production helper 的 matrix 按列主序存储，跨 bin 读取同一 row 需要 stride load（跨步加载）。 | asm 归到 `fpfh.hpp:94-96` 的 `vlse32` / FMA 区域。 | custom bin count 和 custom output layout 不进入 RVV path。 |
| row source（行来源） | 支持 production lookup 后任意有效 row indices。 | `computeFeature` 可能把 surface index 重映射到非连续 SPFH row；不能只支持 dense rows。 | `WeightsSPFHWithRemappedRowsLikeProductionHelper` correctness pass。 | 越界或负 row index 回标量。 |
| 累加机制 | 每个邻居 row 的三段 11 bins 用 RVV `vlse32` 加载，用 FMA 累加到输出 histogram。 | 33-bin 小固定宽度适合 VL chunk（可变向量长度分块），无需额外 staging（暂存）。 | `component_weighted_spfh_33` repeated avg 4.804x。 | 未做 RVV-vs-RVV FMA/no-FMA 消融。 |
| 归一化 | 每段先保留标量 sum，再用 RVV `vfmul` 缩放 11 bins。 | 标量 sum 保持简单和可读；缩放是固定宽度连续写回。 | asm 归到 `fpfh.hpp:114-115`；QEMU correctness pass。 | 若改成 vector reduction（向量规约）需重新验证误差和收益。 |
| 未覆盖组件 | `computePointSPFHSignature` 保持标量。 | pair feature 涉及 `atan2`、`acos`、normalization 和 histogram scatter，语义风险更高。 | `component_spfh_signature` repeated avg 1.010x，只是中性诊断。 | 另开 `spfh-pair-feature-batch` phase 前先做数学 helper 审计。 |

## VL Chunk 流程

一次邻居贡献的 RVV 流程如下：

```text
for each neighbor row:
  weight = 1 / dist
  for segment in f1, f2, f3:
    for bin chunk within 11 bins:
      vl = vsetvl(remaining bins)
      src = vlse32(segment_matrix.data + row + bin * rows, rows * sizeof(float), vl)
      acc = vle32(output + segment_offset + bin, vl)
      out = vfmacc(acc, weight, src, vl)
      vse32(output + segment_offset + bin, out, vl)

for each 11-bin segment:
  sum = scalar sum(output segment)
  scale = sum == 0 ? 0 : 100 / sum
  output segment = RVV multiply by scale
```

这里使用 `vlse32` 是因为 Eigen column-major matrix 中同一 row 的相邻 bin 在内存里相隔 `rows` 个 float；
输出 histogram 是连续 33 个 float，因此累加和归一化写回使用连续 load/store。

## 数值算例

假设只看 f1 的 3 个 bin，两个邻居 row 的 SPFH 值和距离如下：

| neighbor | dist | weight | f1 bins |
| --- | --- | --- | --- |
| row 4 | 2.0 | 0.5 | `[10, 20, 30]` |
| row 9 | 4.0 | 0.25 | `[8, 12, 20]` |

加权累加得到：

```text
[10, 20, 30] * 0.5 + [8, 12, 20] * 0.25 = [7, 13, 20]
```

该段 sum 为 40，归一化 scale 为 2.5，最终 f1 segment 为：

```text
[17.5, 32.5, 50.0]
```

RVV helper 对 11 个 bin 分 chunk 做同样的加载、FMA 和缩放；分段 sum 仍按标量顺序计算，避免把当前 patch
同时变成 reduction 语义变更。

## Fallback 矩阵

| 条件 | 行为 | 维护边界 |
| --- | --- | --- |
| 非 `__RVV10__` 构建 | 不包含 RVV helper，执行原标量路径。 | 公开 API 和非 RVV 行为不变。 |
| `indices.size()!=dists.size()` | helper 返回 false，入口继续标量。 | 保持原入口 assert / 标量语义边界。 |
| SPFH matrix 不是 11 bins | helper 返回 false。 | custom bin count 不使用当前 33-bin RVV helper。 |
| 三个 SPFH matrix row count 不一致 | helper 返回 false。 | 避免跨矩阵 stride 读错。 |
| row index 负值或越界 | helper 返回 false。 | raw invalid row 不进入 RVV。 |
| row index 非连续但有效 | RVV helper 支持。 | production lookup 的 remap 形态已覆盖。 |
| `dists[idx] == 0.0f` | 跳过该邻居贡献。 | 与原标量主体保持一致。 |

## Bench 与证据

证据主归属：

- Phase 002 result：`test-rvv/features/fpfh/doc/phases/002-weighted-spfh-33-production-probe/result.zh.md`
- evaluation：`test-rvv/features/fpfh/doc/fpfh-evaluation.zh.md`
- manifest：`test-rvv/features/fpfh/log/board/pi1-production-weighted/repeated/evidence_manifest.json`
- Doctor：`test-rvv/features/fpfh/log/board/pi1-production-weighted/repeated/evidence_doctor.md`

板卡 repeated 数据集是 `synthetic fpfh point-normal grid side=48 points=2304 k=32`，每 run 8 iterations、2 warmup，共 5 runs。

| case | 入口 / 计时边界 | evidence role（证据角色） | avg speedup | 证明点 | 不能证明 |
| --- | --- | --- | --- | --- | --- |
| `component_weighted_spfh_33` | repeated production `weightPointSPFHSignature` | `production_detail` | 4.804x | 当前 RVV helper 在 detail boundary（细节边界）强正向。 | 公开入口所有数据集、OMP、pair-feature 优化。 |
| `public_fpfh_k` | `FPFHEstimation::compute`，包含 KSearch | `production_public` | 1.262x | 当前合成公开入口下收益没有被 search 完全稀释。 | 真实 workload 全集或其它点类型。 |
| `candidate_weighted_spfh_dense_rows` | test-only dense-row candidate | `diagnostic` | 1.858x | 历史诊断对照。 | 不能作为 production adoption evidence。 |
| `component_spfh_signature` | `computePointSPFHSignature` component | `production_shaped_diagnostic` | 1.010x | SPFH 构建组件当前基本中性。 | 不能写成 pair-feature RVV 成功。 |

Evidence Doctor（证据体检）结果为 `Errors=0, Warnings=0, Suggestions=9`。Suggestions 主要是缺少 taskset /
governor / freq / temperature 等环境 metadata（元数据）和 binary identity（二进制身份）字段；当前无 Error/Warning，
但如果后续出现方向反转，应先补齐 metadata 和 binary hash 后重跑。

## 正确性与高效性证据链

| 层级 | 证据 | 结论 |
| --- | --- | --- |
| correctness（正确性） | `make -B -C test-rvv/features/fpfh run_test_compare`：Std/RVV 6/6 pass。 | RVV path 与标量 reference 在当前测试范围内一致。 |
| row semantics（行语义） | remapped row regression 使用 `{0,12,4,15,2,9,6}`。 | production lookup 后非连续有效 row index 可走 RVV，不要求 dense row。 |
| asm attribution（反汇编归属） | `addr2line` 将关键 RVV 指令映射到 `features/include/pcl/features/impl/fpfh.hpp:94-96,114-115`。 | 生产 helper 真实编入 bench，不是 test-only candidate 或 stale installed header。 |
| performance（性能） | Milkv-Jupiter repeated board：detail 4.804x，public 1.262x，均 0/5 degradation。 | 当前有界生产补丁值得保留。 |
| boundary（边界） | matrix / bin / row gate 明确；非覆盖路径回标量。 | 结论只覆盖 weighted 33-bin helper，不覆盖完整 FPFH family。 |

QEMU timing（仿真器计时）没有性能意义，本 topic 的性能结论只使用板卡 repeated 数据。

## Traceability Map

| 符号 / 文件 | 层级 | 作用 | 调用者 / 上游入口 | 被调用者 / 下游消费者 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- | --- | --- |
| `pcl::detail::weightFPFHSignature33RVV` | production RVV helper | 33-bin weighted FPFH RVV 累加和归一化。 | `weightPointSPFHSignature` | `fpfh_histogram` | adopted production implementation | `features/include/pcl/features/impl/fpfh.hpp` |
| `FPFHEstimation::weightPointSPFHSignature` | production dispatch / fallback | 尝试 RVV helper，失败回原标量主体。 | `computeFeature`, FPFH OMP caller | `FPFHSignature33` output copy | dispatch / fallback boundary | `features/include/pcl/features/impl/fpfh.hpp` |
| `FPFHEstimation::computeFeature` | production public entry | 公开 FPFH 主路径，包含 search 和 weighted FPFH。 | `Feature::compute()` | weighted helper | public dilution check | `features/include/pcl/features/impl/fpfh.hpp` |
| `src/test_fpfh.cpp` | correctness tests | 对拍 weighted helper、remapped rows 和 public finite / NaN 语义。 | `run_test_compare`, `board_smoke` | gtest | correctness gate | `test-rvv/features/fpfh/src/test_fpfh.cpp` |
| `src/bench_fpfh.cpp` | bench wrapper | 生产 detail、public 和诊断 case 的 Std/RVV compare。 | board targets | analyze logs | board performance | `test-rvv/features/fpfh/src/bench_fpfh.cpp` |
| `generate_fpfh_evidence_manifest.py` | analysis script | 生成 Evidence Doctor manifest。 | `evidence_doctor_repeated` | `evidence_doctor.py` | evidence validation | `test-rvv/features/fpfh/script/generate_fpfh_evidence_manifest.py` |
| Phase 002 result | phase closeout | 本轮 PI5 事实、命令和 EvidenceDecision 主归属。 | reviewer / worker | evaluation / this doc | recovery pointer | `test-rvv/features/fpfh/doc/phases/002-weighted-spfh-33-production-probe/result.zh.md` |
| evaluation | decision audit | 候选取舍、fallback matrix 和 doc ownership 主归属。 | reviewer / worker | this doc | production decision audit | `test-rvv/features/fpfh/doc/fpfh-evaluation.zh.md` |

## 生产 closeout

| area | 当前状态 |
| --- | --- |
| production file | `features/include/pcl/features/impl/fpfh.hpp` |
| public API | 不变。 |
| RVV compile gate | `__RVV10__`。 |
| helper | `pcl::detail::weightFPFHSignature33RVV`。 |
| fallback | helper 返回 false 后执行原标量主体。 |
| test scope | `PointNormal -> FPFHSignature33`, `float`, 33 bins, synthetic public KSearch dataset。 |
| evidence | QEMU 6/6, asm attribution, board repeated, Evidence Doctor 0/0/9。 |
| rollback boundary | 删除 RVV include、detail helper 和入口短路即可回到原标量行为；不涉及公开 API。 |

## 后续方向

后续优化应另开 phase，不混入当前 production closeout：

- `spfh-pair-feature-batch`：Phase 003 已暂缓直接实现。恢复前先补 caller-specific bin-stability test、
  boundary-margin fallback 和 histogram scatter staging 设计。
- generic point type expansion：补 traits/layout gate、fallback tests、asm、board 和 Doctor。
- OMP FPFH：单独验证 threaded public entry 和调度归因。
- board metadata：若后续性能方向不稳定，先补 taskset、governor、freq、temperature 和 binary hash。
