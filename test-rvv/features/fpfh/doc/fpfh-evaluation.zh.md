# FPFH 函数级评估

## 范围和目标源码

目标源码是 `features/include/pcl/features/impl/fpfh.hpp`。公开入口来自
`pcl::FPFHEstimation<PointInT, PointNT, PointOutT>`，调用链是 `Feature::compute()` 完成输入检查和输出分配后进入
protected `computeFeature(PointCloudOut&)`。

本 topic 当前覆盖三个函数族：

| 函数 / 函数族 | 作用 | 输入 / 输出状态 | 与主流程关系 | RVV 判断 |
| --- | --- | --- | --- | --- |
| `computePointSPFHSignature` | 对 query point（查询点）和邻域点计算三组 angular feature（角特征）直方图。 | 读 cloud / normals / neighborhood indices，写 `hist_f1/f2/f3(row, bin)`。 | SPFH（Simple Point Feature Histogram，简化点特征直方图）构建热点。 | Phase 003 已审计并暂缓；恢复前需要 bin-stability（分箱稳定性）和 histogram scatter 语义证据。 |
| `weightPointSPFHSignature` | 按邻居距离权重合成最终 33-bin FPFH descriptor（描述子）。 | 读 SPFH matrix rows 和 `dists`，写 33-bin `fpfh_histogram`。 | 每个输出点都会调用，是低维固定 bin 累加热点。 | Phase 002 已接入有界 RVV production path，并通过 PI5 证据支持保留。 |
| `computeSPFHSignatures` | 收集所有需要 SPFH 的 surface index，并为每行调用 `computePointSPFHSignature`。 | 读 `indices_`、`surface_`、search method，写 SPFH matrix 和 lookup。 | 连接公开入口和 SPFH helper。 | 搜索和 set 去重是标量控制流，当前不作为首选 RVV 目标。 |
| `computeFeature` | 对每个输入 index 搜邻域、重映射 SPFH row、加权合成输出 descriptor。 | 写 `PointOutT::histogram[33]` 和 `output.is_dense`。 | 公开主路径。 | `public_fpfh_k` 证明当前 weighted helper 收益在合成 KSearch 数据集上没有被完全稀释。 |
| `features/src/pfh.cpp::computePairFeatures` | Darboux frame（Darboux 坐标系）中计算 f1/f2/f3/f4。 | 读两个点和 normal，写四元组。 | FPFH / PFH / VFH shared helper（共享辅助函数）。 | 本文件无 batch API；只能通过 caller-shaped helper（调用方形态 helper）评估，不能直接改共享语义。 |

## 标量流程与 RVV 流程对照

标量公开流程先调用 `computeSPFHSignatures`，为输入点及其邻居构建 SPFH matrix。每个 SPFH row 遍历邻域
indices：跳过 query point 本身，调用 `computePairFeatures` 得到 f1/f2/f3，再把三个值分别映射到 11-bin
histogram（直方图）并累加 `100 / (k - 1)`。随后 `computeFeature` 对每个输出点重新搜索邻域，把邻域
surface index 映射到 SPFH matrix row，再由 `weightPointSPFHSignature` 按 `1 / dist` 累加三段 11-bin
histogram，最后把每段归一化到 100 并复制到 `FPFHSignature33`。

Phase 002 的 RVV 路径只接管最后的 weighted FPFH 合成：

1. `weightPointSPFHSignature` 在 `__RVV10__` 下调用 `pcl::detail::weightFPFHSignature33RVV`。
2. helper 先检查 `indices.size()==dists.size()`、三段 SPFH matrix 均为 11 bins、row count 一致、所有 row index 在范围内。
3. 每个邻居 row 按 Eigen column-major layout（列主序布局）用 `vlse32` 跨步读取 11 个 bin，用 FMA（融合乘加）累加到连续 33-bin 输出。
4. 三个 11-bin segment（分段）分别计算标量 sum，再用 RVV `vfmul` 做分段归一化。
5. gate 不满足时返回 false，入口继续执行原标量主体。

`computePointSPFHSignature`、KdTree search、SPFH lookup、输出 `PointOutT` 复制和 OMP 路径仍保持既有实现。
Phase 003 已确认 common RVV math 中存在 `acos_RVV_f32m2` 和 `atan2_RVV_f32m2`，但它们是近似 helper；SPFH 的
11-bin 输出对 bin boundary（分箱边界）敏感，因此不能直接把 pair-feature 标量 libm 链替换为 RVV 近似链。

## Traceability Map

| 符号 / 文件 | 层级 | 作用 | 调用者 / 上游入口 | 被调用者 / 下游消费者 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- | --- | --- |
| `FPFHEstimation::computeFeature` | production public entry | 真实公开入口主流程，包含 search、SPFH lookup、weighted FPFH 和输出复制。 | `Feature::compute()` | `computeSPFHSignatures`、`weightPointSPFHSignature` | production boundary（生产边界）事实来源 | `features/include/pcl/features/impl/fpfh.hpp` |
| `pcl::detail::weightFPFHSignature33RVV` | production RVV helper | 对默认 33-bin weighted FPFH 执行 RVV 累加和归一化。 | `weightPointSPFHSignature` | `FPFHSignature33` descriptor copy | adopted production implementation（已采用生产实现） | `features/include/pcl/features/impl/fpfh.hpp` |
| `weightPointSPFHSignature` | production dispatch / fallback | RVV gate 成功时短路返回；否则执行原标量 weighted FPFH。 | `computeFeature` / `FPFHEstimationOMP` | output descriptor copy | dispatch / fallback coverage（分流和回退覆盖） | `features/include/pcl/features/impl/fpfh.hpp` |
| `computePointSPFHSignature` | production helper | 为一个 query point 构造 f1/f2/f3 SPFH histograms。 | `computeSPFHSignatures` | `computePairFeatures` | deferred component target（暂缓组件目标） | `features/include/pcl/features/impl/fpfh.hpp` |
| `computePairFeatures` | production shared helper | 计算 Darboux frame 点对特征。 | FPFH / PFH / VFH family callers | SPFH / PFH histogram bins | shared math source（共享数学来源） | `features/src/pfh.cpp` |
| `include/fpfh.h` | RVV test asset | topic-local 聚合入口。 | test / bench sources | `include/impl/*.hpp` | test support boundary（测试支撑边界） | `test-rvv/features/fpfh/include/fpfh.h` |
| `include/impl/fpfh_reference.hpp` | RVV test asset | test-only scalar reference 和 synthetic input。 | `src/test_fpfh.cpp`, `src/bench_fpfh.cpp` | 无 production consumer | correctness reference（正确性参考） | `test-rvv/features/fpfh/include/impl/fpfh_reference.hpp` |
| `include/impl/fpfh_weighted_candidate.hpp` | RVV test asset | Phase 001 test-only dense-row weighted SPFH candidate。 | `src/test_fpfh.cpp`, `src/bench_fpfh.cpp` | 无 production consumer | historical diagnostic（历史诊断） | `test-rvv/features/fpfh/include/impl/fpfh_weighted_candidate.hpp` |
| `src/test_fpfh.cpp` | correctness tests | 对拍 SPFH、weighted FPFH、remapped rows 和公开入口 finite / NaN 语义。 | `run_test_compare`, `board_smoke` | gtest | QEMU / board correctness（正确性） | `test-rvv/features/fpfh/src/test_fpfh.cpp` |
| `src/bench_fpfh.cpp` | bench wrapper | component 和 public case 的 Std/RVV compare。 | `run_bench_*` / board targets | analyze script | performance evidence（性能证据） | `test-rvv/features/fpfh/src/bench_fpfh.cpp` |
| `script/generate_fpfh_evidence_manifest.py` | evidence wrapper | 把 repeated board summary 转成 Evidence Doctor manifest。 | `evidence_doctor_repeated` | `evidence_doctor.py` | evidence validation（证据校验） | `test-rvv/features/fpfh/script/generate_fpfh_evidence_manifest.py` |
| `doc-rvv/features/fpfh-RVV.zh.md` | production long-term doc | 记录当前 adopted production 行为、fallback、证据链和长期边界。 | reviewer / maintainer | production source and test-rvv docs | production maintenance doc（生产维护文档） | `doc-rvv/features/fpfh-RVV.zh.md` |

## 实现方式审计

| 实现维度 | 当前状态 | 证据 | 边界 / 恢复条件 |
| --- | --- | --- | --- |
| dispatch / fallback | `adopted_with_bounded_scope`。`__RVV10__` 下先尝试 `weightFPFHSignature33RVV`，失败回标量主体。 | QEMU 6/6，remapped row test，asm 归到 `fpfh.hpp`。 | 非 RVV 构建、非 33-bin、row index 越界、matrix row mismatch 等路径回标量。 |
| layout / traits gate | 覆盖 `PointNormal -> FPFHSignature33`、`float`、三段 11 bins、Eigen column-major matrix、contiguous output。 | `src/test_fpfh.cpp`, `src/bench_fpfh.cpp`, board repeated。 | 泛型点类型、custom output、`Scalar=double` 未覆盖。 |
| row source / search | helper 支持 production lookup 后任意有效 row index；public case 使用 KSearch synthetic cloud。 | 新增 remapped row regression；`public_fpfh_k` avg 1.262x。 | 真实 workload、surface != input 的更多组合和 OMP 未覆盖。 |
| staging / reduction | weighted helper 无额外 staging；每个 row 的 11-bin 段用 stride load + FMA 累加，归一化 sum 仍用标量求和，再 RVV scale。pair-feature scatter 暂缓。 | `addr2line` 映射到 `fpfh.hpp:94-96,114-115`；detail avg 4.804x；Phase 003 result。 | 若未来改成 vector reduction（向量规约）或实现 histogram scatter，需要重新做误差、bin-stability、asm 和 board 证据。 |
| formula / FMA | 采用 `acc + weight * src` 的 FMA intrinsic，输出后按原逻辑分段归一化到 100。 | QEMU same-chain pass，board detail strong positive。 | 未做 RVV-vs-RVV FMA/no-FMA detail A/B；当前没有已有 adopted RVV family，故不需要 family-selection A/B 才能保留。 |
| production scope | weighted FPFH 合成已接 production；其它函数族仍标量。 | Phase 002 result。 | `computePointSPFHSignature` 和 OMP 必须另开 phase。 |

## 测试和 Bench 计划

| 测试 / target | 层级 | 作用 |
| --- | --- | --- |
| `run_test_compare` | unit / production-shaped correctness（单元与生产形态正确性） | Std / RVV 构建都运行 SPFH reference 对拍、weighted SPFH 对拍、remapped row regression、public FPFH finite descriptor 和 invalid query NaN 语义。 |
| `dump_bench_rvv` | asm attribution（反汇编归因）入口 | 构建 RVV bench 并导出向量指令；Phase 002 要求指令归到 `features/include/pcl/features/impl/fpfh.hpp`。 |
| `board_smoke` | board correctness + board bench smoke | 板卡执行 RVV test 和一次 Std/RVV bench compare，证明可运行和日志形状。 |
| `board_repeated` | board performance evidence（板卡性能证据） | 5-run repeated board compare，作为 EvidenceDecision 输入。 |
| `evidence_doctor_repeated` | Evidence Doctor（证据体检） | 对 repeated manifest 输出 Errors / Warnings / Suggestions，并决定是否降级证据。 |

Bench case 字典：

| case | 计时边界 | 能证明 | 不能证明 |
| --- | --- | --- | --- |
| `component_spfh_signature` | 固定 synthetic neighborhood 上重复调用 `computePointSPFHSignature`。 | SPFH pair-feature / histogram 组件是否值得继续做 RVV candidate。 | 本阶段 weighted helper 收益、真实 search 成本、production dispatch。 |
| `component_weighted_spfh_33` | synthetic SPFH matrix 上重复调用 production `weightPointSPFHSignature`。 | 33-bin 加权累加与归一化 production detail 是否有收益。 | pair-feature 成本、真实工作负载全集、OMP。 |
| `candidate_weighted_spfh_dense_rows` | synthetic dense sequential SPFH rows 上重复调用 test-only `weightPointSPFHDenseRowsRVV`。 | Phase 001 dense-row RVV candidate 的历史诊断对照。 | production adoption；非连续 row remap；public entry speedup。 |
| `public_fpfh_k` | `FPFHEstimation::compute` public path，包含 KdTree search 和输出 descriptor。 | 当前 weighted helper 收益能否转化为合成公开入口收益。 | 泛型点类型、OMP、真实数据分布、其它 RVV family 最优性。 |

## 当前验证结果

### QEMU correctness

```bash
make -B -C test-rvv/features/fpfh run_test_compare
```

Std 6/6 pass，RVV 6/6 pass。证据路径：

- `test-rvv/features/fpfh/log/qemu/run_test_std.log`
- `test-rvv/features/fpfh/log/qemu/run_test_rvv.log`

### 反汇编

```bash
make -B -C test-rvv/features/fpfh dump_bench_rvv
```

`test-rvv/features/fpfh/build/asm/riscv/bench_fpfh_rvv.full.asm` 中
`weightPointSPFHSignature [clone .isra.0]` 命中本地 `fpfh.hpp`。`addr2line` 将 `0x307aa`、`0x307b4`、
`0x307b8` 映射到 `fpfh.hpp:94-96`，将 `0x3097a` 到 `0x309a8` 映射到 `fpfh.hpp:114-115`。

### 板卡 repeated

```bash
make -C test-rvv/features/fpfh board_repeated \
  REPEATED_BOARD_OUTPUT_DIR=log/board/pi1-production-weighted/repeated
```

| case | evidence role | avg speedup | speedup values | decision note |
| --- | --- | --- | --- | --- |
| `component_weighted_spfh_33` | `production_detail` | 4.804x | 5.06x, 4.85x, 4.57x, 4.85x, 4.69x | 强正向，采纳主证据。 |
| `public_fpfh_k` | `production_public` | 1.262x | 1.27x, 1.25x, 1.27x, 1.27x, 1.25x | 公开入口正向，说明当前数据集下未被 search 完全稀释。 |
| `candidate_weighted_spfh_dense_rows` | `diagnostic` | 1.858x | 1.93x, 1.86x, 1.80x, 1.87x, 1.83x | 历史诊断对照，不作为采纳证据。 |
| `component_spfh_signature` | `production_shaped_diagnostic` | 1.010x | 1.01x, 1.00x, 1.04x, 1.00x, 1.00x | 基本中性；后续 pair-feature phase 需要重新设计。 |

### Evidence Doctor

```bash
make -C test-rvv/features/fpfh evidence_doctor_repeated \
  REPEATED_BOARD_OUTPUT_DIR=log/board/pi1-production-weighted/repeated \
  EVIDENCE_MANIFEST_REPEATED=log/board/pi1-production-weighted/repeated/evidence_manifest.json \
  EVIDENCE_DOCTOR_REPEATED_MD=log/board/pi1-production-weighted/repeated/evidence_doctor.md \
  EVIDENCE_DOCTOR_REPEATED_JSON=log/board/pi1-production-weighted/repeated/evidence_doctor.json
```

结果：Errors=0，Warnings=0，Suggestions=9。Suggestions 主要是缺少 board environment metadata（环境元数据）、
binary identity（二进制身份）和 `component_spfh_signature` near-threshold（接近阈值）。这些不阻塞当前
weighted helper 采纳，但后续若数值反转，必须先补 metadata 和 binary hash 后复跑。

## 生产接入判断

本 topic 采用的 production decision（生产决策）口径是：只有接入 `features/include/pcl/features/impl/fpfh.hpp`
之后的 repeated board（重复板卡测试）结果显示收益，才把补丁写成 `adopted_with_bounded_scope`。Phase 001
的 test-only dense-row candidate 只用于触发 production probe，不作为采纳证据；正式 `doc-rvv` 文档中的
性能数据也只采用 Phase 002 production boundary 内的板卡结果。

`production_patch_scope`：

- 修改 `features/include/pcl/features/impl/fpfh.hpp`。
- `__RVV10__` 下 include `<riscv_vector.h>`。
- 新增 `pcl::detail::weightFPFHSignature33RVV`。
- `weightPointSPFHSignature` 在进入原标量主体前尝试 RVV helper。

`covered_path`：

- 入口：`weightPointSPFHSignature`，并通过 `public_fpfh_k` 覆盖 `FPFHEstimation::compute` 的合成公开路径。
- 点类型：验证为 `PointNormal -> FPFHSignature33`。
- Scalar / layout：`float`，11+11+11 bins，Eigen column-major SPFH matrix，contiguous 33-bin output。
- 目标硬件：Milkv-Jupiter board。

`fallback_matrix`：

| 条件 | 行为 | 证据 |
| --- | --- | --- |
| 非 RVV 构建 | 不编译 RVV helper，执行原标量主体。 | Std 6/6 pass。 |
| `hist_f1/f2/f3` 不是 11 bins | helper 返回 false，执行标量。 | 源码 gate；custom bin 未作为 RVV 覆盖范围。 |
| `indices.size()!=dists.size()` | helper 返回 false，执行标量。 | 源码 gate；公开路径已有 assert 语义。 |
| row index 越界或负值 | helper 返回 false，执行标量。 | 源码 gate。 |
| row index 非连续但有效 | RVV helper 支持。 | remapped row regression。 |
| `dists[idx]==0.0f` | 与原标量一致跳过该邻居贡献。 | QEMU correctness。 |

`decision_delta`：

Phase 001 的 dense-row candidate 只说明值得进入 production probe；Phase 002 用同一 production boundary
中的 correctness、asm、board repeated 和 Doctor 证据替换诊断证据，结论升级为
`adopted_with_bounded_scope`。没有把 Phase 001 speedup 当作采纳依据。

## 文档发布边界

正式 production 长期主题文档已创建：

- `doc-rvv/features/fpfh-RVV.zh.md`

它只记录当前 adopted weighted FPFH production behavior、fallback、证据链和长期边界。Phase 探索、candidate
搜索空间和下一步计划的主归属仍是：

- `test-rvv/features/fpfh/doc/phases/002-weighted-spfh-33-production-probe/result.zh.md`
- `test-rvv/features/fpfh/doc/phases/optimization-matrix.zh.md`
- `test-rvv/features/fpfh/doc/optimization-roadmap.zh.md`

## 后续方向

当前 adopted patch 没有 phase 内未阻塞缺口，用户已确认可以结束当前 topic 并进入 topic-only commit
（只提交本主题相关产物）流程。Phase 003 已审计 `spfh-pair-feature-batch` 并暂缓直接实现。
恢复该方向前需要：

- 建立 caller-specific scalar/RVV math chain test，比较 pair-order decision、f1/f2/f3 和 bin index。
- 对靠近 bin boundary 的 lane 定义标量 fallback 或保留标量 binning。
- 设计 histogram scatter staging，并证明冲突累加语义与收益。
- 不把 generic point type、OMP 和 `Scalar=double` 扩展混入当前 weighted helper closeout。
