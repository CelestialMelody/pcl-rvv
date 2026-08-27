# GASD 函数级评估

## 范围和目标源码

目标源码是 `features/include/pcl/features/impl/gasd.hpp`。当前公开入口有两个：

- `pcl::GASDEstimation<PointInT, PointOutT>::computeFeature`
- `pcl::GASDColorEstimation<PointInT, PointOutT>::computeFeature`

本轮只做 diagnostic（诊断）评估，不修改 production（生产源码），也不把 QEMU（仿真器）时间写成性能结论。Phase 000 已评估 fixed-grid histogram copy（固定网格直拷贝），Phase 010 已评估 shape sample projection staging（形状样本投影暂存），Phase 020 已评估 color hue / hbin staging（颜色色相 / 直方图 bin 暂存），Phase 030 已评估 trilinear interpolation arithmetic / index staging（三线性插值算术 / 索引暂存），Phase 040 已评估 trilinear staging + scalar flat histogram write（三线性暂存 + 标量扁平直方图写回），Phase 050 已评估 Eigen-backed histogram write probe（使用 `std::vector<Eigen::VectorXf>` 的直方图写回探针），Phase 060 已评估 production-shaped shape combined diagnostic（生产形态 shape 组合诊断），Phase 070 已完成 no-production closeout / profile recovery audit（不接入生产收尾 / 性能剖析恢复条件审计），Phase 080 已补充 public compute profile（公开入口性能剖析）。

## 函数作用速览

| 函数 / 函数族 | 作用 | 输入 / 输出状态 | 与主流程关系 | RVV 判断 |
| --- | --- | --- | --- | --- |
| `GASDEstimation::computeFeature` | 计算 canonical transform 后的 shape descriptor。 | 读取 `surface_`、`indices_` 和 `shape_interp_`，写 `output`。 | 公开入口 `compute()` 的主体。 | `shape_samples_` 投影已有 diagnostic-positive；后续若接近 production 还必须补 histogram 写回和 public dispatch 证据。 |
| `GASDColorEstimation::computeFeature` | 在 shape descriptor 基础上叠加 hue histogram。 | 读取 RGB 样本，写 `output`。 | 公开入口 `compute()` 的主体。 | hue / hbin staging 已有 diagnostic-positive；后续仍需证明 color histogram interpolation 写回。 |
| `computeAlignmentTransform` | 计算 canonical alignment transform。 | 读取 centroid、covariance、eigenvector。 | 每次 compute 调用只做一次。 | 标量边界，不作为 phase 000 主候选。 |
| `addSampleToHistograms` | 将一个样本写入 3D regular grid 的 cell histogram。 | 读取 sample、bin 和 interpolation mode，更新 histograms。 | 逐样本热点。 | 后续 phase 可以再审计 projection / interpolation。 |
| `copyShapeHistogramsToOutput` / `copyColorHistogramsToOutput` | 把内部 histograms 线性拷贝到 descriptor 输出。 | 读写连续 float buffer。 | 生产链路的固定网格写回尾段。 | Phase 000 的 first candidate。 |

## 标量流程与诊断 RVV 流程对照

当前标量链路先做对齐，再对每个样本投影到 3D regular grid（规则三维网格），最后把每个 cell 的 interior bins（内部 bin）线性写到 descriptor 输出。color 分支在 shape 完成后，再把 hue sample 写入另一个 fixed-grid。

Phase 000 只把 fixed-grid copy 这段做成 test-only candidate（测试专用候选）。Phase 010 进一步把 shape sample projection 拆成 staging buffer 对拍，并使用 production-like normalization（从 synthetic cloud 推导 `max_coord` 和 `distance_normalization_factor`）。Phase 020 把 color hue / hbin 拆成 staging buffer，对齐 production 中 `max/min/diff_inv/std::isfinite` 和 RGB 分支语义。这些诊断都不覆盖生产调度、不替代完整 descriptor 计算，也不证明 `addSampleToHistograms` 已经值得 RVV。

## Traceability Map

| 符号 / 文件 | 层级 | 作用 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- |
| `GASDEstimation::computeFeature` | production public entry | shape descriptor 主流程 | production boundary（生产边界） | `features/include/pcl/features/impl/gasd.hpp` |
| `GASDColorEstimation::computeFeature` | production public entry | color descriptor 主流程 | production boundary（生产边界） | `features/include/pcl/features/impl/gasd.hpp` |
| `gasd::copyShapeHistogramsStd` / `gasd::copyShapeHistogramsRVV` | test support candidate | 固定网格直拷贝对拍 | diagnostic helper（诊断 helper） | `test-rvv/features/gasd/include/impl/gasd_copy_candidate.hpp` |
| `gasd::copyColorHistogramsStd` / `gasd::copyColorHistogramsRVV` | test support candidate | hue grid 直拷贝对拍 | diagnostic helper（诊断 helper） | `test-rvv/features/gasd/include/impl/gasd_copy_candidate.hpp` |
| `gasd::projectShapeSamplesStdToBuffers` / `gasd::projectShapeSamplesRVVToBuffers` | test support candidate | shape projection staging 对拍 | diagnostic helper | `test-rvv/features/gasd/include/impl/gasd_reference.hpp`、`include/impl/gasd_copy_candidate.hpp` |
| `gasd::projectColorHueStdToBuffers` / `gasd::projectColorHueRVVToBuffers` | test support candidate | color hue / hbin staging 对拍 | diagnostic helper | `test-rvv/features/gasd/include/impl/gasd_reference.hpp`、`include/impl/gasd_copy_candidate.hpp` |
| `gasd::computeTrilinearInterpolationStdToBuffers` / `gasd::computeTrilinearInterpolationRVVToBuffers` | test support candidate | trilinear `grid_idx/h_idx` 和 8 个权重暂存对拍 | diagnostic helper | `test-rvv/features/gasd/include/impl/gasd_reference.hpp`、`include/impl/gasd_copy_candidate.hpp` |
| `gasd::accumulateTrilinearHistogramStd` / `gasd::accumulateTrilinearHistogramRVVStaged` | test support candidate | flat histogram 写回对拍，RVV 只负责 staging | diagnostic helper | `test-rvv/features/gasd/include/impl/gasd_reference.hpp`、`include/impl/gasd_copy_candidate.hpp` |
| `gasd::accumulateTrilinearHistogramEigenStd` / `gasd::accumulateTrilinearHistogramEigenRVVStaged` | test support candidate | `std::vector<Eigen::VectorXf>` 写回对拍，RVV 只负责 staging | diagnostic helper | `test-rvv/features/gasd/include/impl/gasd_reference.hpp`、`include/impl/gasd_copy_candidate.hpp` |
| `gasd::computeShapeDescriptorTrilinearStd` / `gasd::computeShapeDescriptorTrilinearRVVStaged` | test support candidate | production-shaped shape combined helper，对拍 projection、trilinear Eigen write 和 shape copy 的组合边界 | production-shaped diagnostic helper | `test-rvv/features/gasd/include/impl/gasd_reference.hpp`、`include/impl/gasd_copy_candidate.hpp` |
| `src/test_gasd.cpp` | correctness gate | Std/RVV 同输入对拍 | unit / regression gate（单元 / 回归验收） | `test-rvv/features/gasd/src/test_gasd.cpp` |
| `src/bench_gasd.cpp` | bench wrapper | baseline / candidate timing、public compute profile 和 checksum | diagnostic / production-public profile benchmark（诊断 / 公开生产入口剖析性能测试） | `test-rvv/features/gasd/src/bench_gasd.cpp` |

## 当前诊断证据链

| phase | candidate | board result | Evidence Doctor | decision |
| --- | --- | --- | --- | --- |
| 000 | fixed-grid histogram copy | 5-run median 1.050x，range 1.040x-1.080x | Errors=0，Warnings=0，Suggestions=2 | diagnostic-weak-positive |
| 010 | shape sample projection staging | 5-run median 1.690x，range 1.690x-1.710x，checksum 一致 | Errors=0，Warnings=0，Suggestions=2 | diagnostic-positive |
| 020 | color hue / hbin staging | 5-run median 1.920x，range 1.910x-1.980x，checksum 一致 | Errors=0，Warnings=0，Suggestions=2 | diagnostic-positive |
| 030 | trilinear interpolation arithmetic / index staging | 5-run median 1.950x，range 1.560x-2.030x，checksum 一致 | Errors=0，Warnings=1，Suggestions=2 | diagnostic-positive；需保留长尾波动说明 |
| 040 | trilinear staging + scalar flat histogram write | 5-run median 1.060x，range 1.050x-1.060x，checksum 一致 | Errors=0，Warnings=0，Suggestions=2 | diagnostic-weak-positive；flat 写回后收益大幅收窄 |
| 050 | trilinear staging + scalar Eigen-backed histogram write | 5-run median 0.820x，range 0.800x-0.820x，checksum 一致 | Errors=1，Warnings=0，Suggestions=2 | diagnostic-negative；Error 是 5/5 退化频率，不能作为生产正向证据 |
| 060 | production-shaped shape combined diagnostic | 5-run median 0.590x，range 0.590x-0.600x，checksum 一致 | Errors=1，Warnings=0，Suggestions=2 | production-shaped-diagnostic-negative；Error 是 5/5 退化频率，不能直接推出 rejected |
| 070 | no-production closeout / profile recovery audit | not_applicable | not_applicable | no-production for current staged shape family；后续需 profile、用户授权 probe 或独立 color follow-up |
| 080 | public compute profile audit | shape public median 1.220x，range 1.210x-1.220x；color public median 1.110x，range 1.100x-1.120x；checksum 一致 | 两组均 Errors=0，Warnings=0，Suggestions=2 | profile-only build-level signal；不改变当前 staged shape family 的 no-production 判断 |

## 生产接入判断

`production_topic_doc_applicability`：`not_applicable with evidence`。当前还没有 adopted production behavior，也没有 PI5 production patch；`doc-rvv/features/gasd-RVV.zh.md` 暂不创建。Phase 060 的 stable negative 和 Phase 070 closeout 支持当前 staged shape family 暂不进入 production patch；Phase 080 的 public compute profile 只说明未改 production 的 RVV build 有 build-level profile signal（构建层级性能信号），不能替代 production direct（真实生产路径证据）。

## 遗留风险和下一步

- shape sample projection 只证明 staging buffer，未证明真实 `addSampleToHistograms` 写回。
- color hue 分支只证明 `hue/hbin` staging，未证明真实 color histogram 写回。
- Eigen-backed trilinear histogram write probe 显示该 staged-write 候选在 production-like container layout
  下退化；Phase 060 组合 projection、Eigen write 和 copy 后仍稳定负向。当前 staged shape family 不建议进入 production patch。
- Phase 080 已补充公开入口 profile：shape public compute median 1.220x，color public compute median 1.110x，但该信号不能归因到当前手写 staged shape family。
- 当前 staged shape family 已完成 no-production closeout。后续若继续当前 topic，建议先做更细的 production segment profile，确认完整 `computeFeature` 的 transform、projection、histogram write 和 copy 占比；若要做 production probe，需要用户授权 PI1 scope 和 production direct 证据计划。
