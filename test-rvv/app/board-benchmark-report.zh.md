# test-rvv/app 板卡基准测试说明

本文对应仓库内目录 `output/board/` 下保存的一份板卡实测记录：`pipeline_compare.log`、`hot_compare.log`、`dag_compare.log`（由 `analyze_bench_compare.py` 汇总 Std 与启用 RVV 头路径后的可执行）。

---

## 1. 环境与对比方法

- 运行介质：单板 RISC-V 64（日志中 `--device board rv64`），可执行置于 `/root/pcl-test/app/`，运行时 `LD_LIBRARY_PATH` 指向与交叉编译链路一致的 libpcl 等 `.so`（例如 `/root/pcl-test/lib`）。
- 两组二进制：同源分别编译，`USE_PCL_RVV10=0` 为标准路径；`USE_PCL_RVV10=1` 时定义 `-D__RVV10__`，使与本程序链接的同一套 libpcl 及头文件中凡带 `#ifdef __RVV10__` 的实现（常见于 `common` / `2d` / `sample_consensus` 等 TU）可被编译进来（与开发机 Makefile 注释一致；是否实际走 RVV 还以该库的编译宏为准）。
- 计时口径：每条程序内对「一次完整链路」外层循环计时；先做若干次 warmup，再统计 `Iterations` 次迭代的平均值 `ms/iter`，并给出 `Avg × Iterations` 作为总毫秒（近似）。
- 复现：板卡上使用 `make -f board.mk run_pipeline_compare`、`run_hot_compare`、`run_dag_compare`（需预先 `deploy_*` 与 `deploy_compare_script`）；开发机可复制 `output/board/` 中日志路径或内容。

---

## 2. 三个应用分别在测什么

文档与源码分工：`doc-rvv/` 及以下工程笔记主要用于推导与 RVV 工作备忘；是否具备 RVV 实现对以应用到的 PCL 头/源为准。例如：`pcl::L2_Norm` / `L1_Norm` 见 [norms.hpp](../../common/include/pcl/common/impl/norms.hpp)；`pipelineRvvCommonIntrinsicStrip` 中调用的 `pcl::expf_RVV_*`、`logf_RVV_*`、`atan2_RVV_*`、`acos_RVV_*` 等出自 [common/impl/common.hpp](../../common/include/pcl/common/impl/common.hpp)；2d 算子实现见 [convolution.hpp](../../2d/include/pcl/2d/impl/convolution.hpp)、[morphology.hpp](../../2d/include/pcl/2d/impl/morphology.hpp)、[edge.hpp](../../2d/include/pcl/2d/impl/edge.hpp)；SAC 见如 [impl/sac_model_plane.hpp](../../sample_consensus/include/pcl/sample_consensus/impl/sac_model_plane.hpp)、[impl/sac_model_normal_plane.hpp](../../sample_consensus/include/pcl/sample_consensus/impl/sac_model_normal_plane.hpp)。

本节拆成两张表：可走 RVV（含 SAC 等仅部分 API 带 `__RVV10__` 者）与不走本仓库 RVV / Eigen / 屏障 / 本 TU 手写；每张表内仍按对应 `pipeline*.cpp` 单次迭代中的出现先后排列。`pipelineRvvCommonIntrinsicStrip` 仅在 `#ifdef __RVV10__` 存在。行旁 `[未 RVV]` 多指 filters/KdTree/变换等 TU，不否认 `pcl/2d`、SAC impl 中已有宏分支。

单次迭代分段计时的 stdout 与各程序内 `pcl_test_rvv_app::StageAccumulator`、`pipeline_benchmark_stage_labels.hpp`（`kBenchStagePIP*`、`kBenchStageHOT*`、`kBenchStageDAG*`）同源。每条标签**首行**为 `[PIPnn] … —` 形式的一句阶段说明，**续行**按 `common/centroid`、`common/common`、`common/norms`、`filters/` 等模块分段列出所测函数（与头文件路径对应；不含 `pcl::` 前缀）。下文「计时阶段」表给索引；完整字符串以头文件为准。

开启 `-v` / `--verbose` 时，**仅首次进入**计时体会向 stderr 打印各阶段（前缀 `[pipeline]` / `[pipeline_hot]` / `[pipeline_dag]`），内容为**人读短句**，列出本段实际调用的主要 API（与 stdout 长标签不必逐字相同）。

`-v/--verbose` 输出里 `end:` 等收尾字符串不计入 `record()`。

### 2.1 `pipeline_app_std` / `pipeline_app_rvv`

- 源码：`pcl_pipeline_app.cpp`（与本说明同目录），`void pipelineOnce(...)`。
- 语义：覆盖面最广；一次迭代整链 `wall-clock`。KdTree、体素等常占大头，全链路倍率受 Amdahl 限制。

#### 计时阶段标签（stdout = `pipeline_benchmark_stage_labels.hpp`）

| 阶段要点（首行 + 模块列） | `record(i)` | `-v` stderr（节选；前缀 `[pipeline] `） | 说明 |
|:-----|:-----------:|:-----------------------------------------------------|:----------------------|
| `[PIP01] raw 点云：common 全段…` → `common/centroid`, `common/common`, `common/norms`, `common/gaussian`, RVV 初等 | 0 | `compute3DCentroid` … `acos_RVV_f32m2` 等 | raw 上 full common + intrinsic |
| `[PIP02] 标量占位` → `getAngle3D` | 1 | `getAngle3D only (scalar…)` | 占位，无 RVV 专项 |
| `[PIP03] 体素滤波` → `VoxelGrid::filter` 等 | 2 | `VoxelGrid::filter`；稀疏则 `copyPointCloud` | filters |
| `[PIP04] 半径法线…` → `NormalEstimation` + `KdTree` | 3 | `NormalEstimation::compute + KdTree…` | features |
| `[PIP05] 刚体变换` → `transformPointCloud` | 4 | `transformPointCloud (rigid…)` | transform |
| `[PIP06] 预制强度图上的 2d` → `Convolution` / `Morphology` / `Edge` | 5 | `copyPointCloud(img2d_master)` → 2d 链 | 与滤波点云数据源独立 |
| `[PIP07] 专用 SAC 点云` → `SampleConsensusModel*::…` | 6 | `countWithinDistance` / `selectWithinDistance` / `getDistancesToModel` | `cloud_sac` |

#### 测试工作流程

- `[PIP01]` 见头文件 `kBenchStagePIP01` 分段列表：centroid、cov、minmax、maxdist、范数、maxSegment、`GaussianKernel`@gauss_gray、面积、以及 `common/common` 下列出的 `expf_RVV_f32m2` 等（仅编译 `__RVV10__` 时）。
- `[PIP02]` `getAngle3D` 标量一次——不计入 RVV 对比语义，占位。
- `[PIP03]` … `[PIP07]` 分别见 `kBenchStagePIP03` … `kBenchStagePIP07`（模块分段与同目录头文件一致）。
- `end: iteration complete`——本迭代计时体结束（非 `record()` 段）。

#### 可走 RVV

| 名称 | 作用 |
|:-----|:-----|
| `pcl::compute3DCentroid`、`pcl::computeMeanAndCovarianceMatrix` | raw 云重心与协方差 |
| `pcl::getMinMax3D`、`pcl::getMaxDistance` | 包围盒 / 最远点信息 |
| 循环 `pcl::L2_Norm`、`pcl::L1_Norm`、`doNotOptimize(acc)`（[norms.hpp](../../common/include/pcl/common/impl/norms.hpp)） | `norm_repeat` 次范数累积 |
| `pcl::getMaxSegment` | 线段云直径 |
| `GaussianKernel::convolveRows`、`convolveCols`、`doNotOptimize(outs)`（[gaussian.cpp](../../common/src/gaussian.cpp)） | `gauss_gray` |
| `pcl::calculatePolygonArea`、`doNotOptimize(pa)`（[common.hpp](../../common/include/pcl/common/impl/common.hpp)） | XY 多边面积 |
| `#ifdef __RVV10__`：`pipelineRvvCommonIntrinsicStrip`（`expf_RVV_*` 等） | [common.hpp](../../common/include/pcl/common/impl/common.hpp)；未定义宏则本 TU 无此调用 |
| `Convolution::filter`、`Morphology::erosionGray`、`Edge::detectEdgeSobel` | [2d impl](../../2d/include/pcl/2d/impl/convolution.hpp) |
| `SampleConsensusModelPlane`、`SampleConsensusModelNormalPlane`（若干 distance API） | （局部）RVV（[impl](../../sample_consensus/include/pcl/sample_consensus/impl/sac_model_plane.hpp)） |

与「计时阶段」对应关系：`[PIP01]` 覆盖本表自上而下至 `pipelineRvvCommonIntrinsicStrip` 行前；`[PIP06]` 对应 Convolution/Morphology/Edge；`[PIP07]` 对应 SAC 两行。

#### 不走本仓库 RVV / 占位 / 屏障

| 名称 | 作用 |
|:-----|:-----|
| `Eigen::Matrix3f::Zero`、`Eigen::Vector4f::Zero` | 工作区清零 |
| `doNotOptimize(cov.trace())` | 屏障 |
| `doNotOptimize(mx.x+fmax[…])` | 屏障 |
| `pcl::getAngle3D` | 占位 |
| `VoxelGrid::filter`；点过少则 `pcl::copyPointCloud` | `filters/` 无 `__RVV10__` |
| `NormalEstimation::compute`、`search::KdTree` | 法线 + 邻域检索 |
| `pcl::transformPointCloud` | 刚体变换 |
| `pcl::copyPointCloud(*img2d_master, ...)` | 强度图拷贝 |
| `doNotOptimize(nf/xformed/dbuf…)` | 末段屏障 |

可走/不走 API 所在计时标签见 §2.1「计时阶段」表：`[PIP02]`、`[PIP03]`…与「不走」表从左到右按迭代顺序对齐。

整条路径仍以邻域查找、滤波、分支与内存摊薄 norms 与 2d 的相对收益，全链路 `std/rvv` 倍率常低于单项微基准。

---

### 2.2 `pipeline_hot_std` / `pipeline_hot_rvv`

- 源码：`pcl_pipeline_hot_rvv.cpp`（同目录），`void pipelineHotOnce(...)`。
- 语义：摘掉体素、KdTree+法线、变换、强度图整条 2d、SAC、`getAngle3D`；典型毫秒级。

#### 计时阶段标签（stdout = `pipeline_benchmark_stage_labels.hpp`）

| 阶段要点 | `record(i)` | `-v` stderr（前缀 `[pipeline_hot] `） | 说明 |
|:-----|:-----------:|:-------------------------------------|:---------------------|
| `[HOT01] raw common 热区…` → 各 `common/*` 行（无 intrinsic） | 0 | 列出 centroid…`calculatePolygonArea`；注明无 voxel 等 | 与 PIP01 common 段对齐，拆出独立阶段 |
| `[HOT02] RVV 初等条带…` → `common/common` RVV 四函数 | 1 | `expf_RVV_f32m2` …（仅 `__RVV10__`） | `record(1)` 仅含 strip |

#### 测试工作流程

- `[HOT01]` 见 `kBenchStageHOT01`：与 §2.1 的 common 热点一致但**无** `getAngle3D`、滤波及后续；`[HOT02]` 见 `kBenchStageHOT02`：`__RVV10__` 下 RVV 四函数（计时独立为 `record(1)`）。
- `end: iteration complete (hot; no voxel/normals/transform/2d/SAC/getAngle3D)`——热区单次迭代收尾（非 `record()` 段）。

#### 可走 RVV

| 名称 | 作用 |
|:-----|:-----|
| `pcl::compute3DCentroid`、`pcl::computeMeanAndCovarianceMatrix` | raw 云统计 |
| `pcl::getMinMax3D`、`pcl::getMaxDistance` | 包围盒与最远点 |
| 循环 `pcl::L2_Norm`、`pcl::L1_Norm`、`doNotOptimize(acc)` | [norms.hpp](../../common/include/pcl/common/impl/norms.hpp) |
| `pcl::getMaxSegment` | 线段云直径 |
| `GaussianKernel::convolveRows`、`convolveCols`、`doNotOptimize(outs)` | `gauss_gray` |
| `pcl::calculatePolygonArea`、`doNotOptimize(pa)` | 多边面积 |
| `#ifdef __RVV10__`：`pipelineRvvCommonIntrinsicStrip` | [common.hpp](../../common/include/pcl/common/impl/common.hpp) |

`[HOT01]`：本表除最后一行外的全部条目；`[HOT02]`：`pipelineRvvCommonIntrinsicStrip`。

#### 不走本仓库 RVV / 屏障

| 名称 | 作用 |
|:-----|:-----|
| `Eigen::Matrix3f::Zero`、`Eigen::Vector4f::Zero` | 工作区清零 |
| `doNotOptimize(cov.trace())` | 屏障 |
| `doNotOptimize(mx.x+fmax[…])` | 屏障 |

不包含：`getAngle3D`、`VoxelGrid`、`NormalEstimation`/`KdTree`、`transformPointCloud`、`Convolution`/`Morphology`/`Edge`、`SampleConsensus*`。

---

### 2.3 `pipeline_dag_std` / `pipeline_dag_rvv`

- 源码：`pcl_pipeline_dag.cpp`（同目录），`void pipelineDagOnce(...)`。
- 语义：单链 DAG；单次迭代可比 hot 更重（板卡快照曾达秒级）。

#### 计时阶段标签（stdout = `pipeline_benchmark_stage_labels.hpp`）

| 阶段要点 | `record(i)` | `-v` stderr（前缀 `[pipeline_dag] `） | 说明 |
|:-----|:-----------:|:-------------------------------------|:---------------------|
| `[DAG01] 体素滤波` | 0 | `VoxelGrid::filter`；稀疏回退 | filters |
| `[DAG02] cloud_f 上 common` | 1 | centroid…`makePolygonFromFilteredExtent`+`calculatePolygonArea` | 滤波后点云 |
| `[DAG03] RVV 初等条带` | 2 | 四 `*_RVV_f32m2`（scratch 非空且 `__RVV10__`） | 独立阶段 |
| `[DAG04] 半径法线…` | 3 | `NormalEstimation` + `KdTree` on `cloud_f` | features |
| `[DAG05] 变换 + 旋法线` | 4 | `transformPointCloud`；`normals_f`→`normals_t` | Eigen 标量环 |
| `[DAG06] Z 栅格 + 高斯` | 5 | `rasterizeZToIntensityGrid`；`GaussianKernel` | bench + gaussian |
| `[DAG07] 强度栅格 + 2d` | 6 | 再栅格；`Convolution`/`Morphology`/`Edge` | 第二路栅格 |
| `[DAG08] 末端 SAC` | 7 | 每轮构造 `ModelPlane`/`ModelNormalPlane` + 距离 API | `cloud_t`,`normals_t` |

#### 测试工作流程

- `[DAG01]` 见 `kBenchStageDAG01`——先得 `cloud_f`；`-v` 打 `cloud_f points`。
- `[DAG02]` 见 `kBenchStageDAG02`——全体在滤波后点云上。
- `[DAG03]` 见 `kBenchStageDAG03`——仅当 scratch 非空且 `__RVV10__`。
- `[DAG04]`/`[DAG05]`/`[DAG06]`/`[DAG07]`/`[DAG08]` 分别对应头文件分段与上表 `-v` 列。

#### 可走 RVV

| 名称 | 作用 |
|:-----|:-----|
| `pcl::compute3DCentroid`、`pcl::computeMeanAndCovarianceMatrix` | `cloud_f` 统计 |
| `pcl::getMinMax3D`、`pcl::getMaxDistance`、`pcl::getMaxSegment` | 几何摘要 |
| `pcl::L2_Norm`、`pcl::L1_Norm`（32× 外乘，`dfeat=min(128,n)`） | [norms.hpp](../../common/include/pcl/common/impl/norms.hpp) |
| `pcl::calculatePolygonArea`、`doNotOptimize(parea)` | 面积 |
| `#ifdef __RVV10__`：`pipelineRvvCommonIntrinsicStrip`（scratch 非空） | [common.hpp](../../common/include/pcl/common/impl/common.hpp)；首迭 `-v` 打印 `optional: ifdef __RVV10__ strip(common.hpp intrinsics)` |
| `GaussianKernel::convolveRows`/`convolveCols`（`zimg`） | [gaussian.cpp](../../common/src/gaussian.cpp) |
| `Convolution::filter`、`Morphology::erosionGray`、`Edge::detectEdgeSobel` | [2d impl](../../2d/include/pcl/2d/impl/convolution.hpp) |
| `SampleConsensusModelPlane`、`SampleConsensusModelNormalPlane` 与距离 API | （局部）RVV |

「可走 RVV」表按 API 汇总；与计时标签的对应为：`[DAG02]` common+polygon，`[DAG03]` intrinsic，`[DAG06]` Z 栅格+Gaussian，`[DAG07]` raster-I + pcl::2d，`[DAG08]` SAC。`[DAG01]` / `[DAG04]` / `[DAG05]` 以「不走」表为主。

#### 不走本仓库 RVV / 本 TU / 屏障

| 名称 | 作用 |
|:-----|:-----|
| `VoxelGrid::filter`；过少点则 `pcl::copyPointCloud` | 体素 / 回退 |
| `Eigen::Matrix3f::Zero`、`Eigen::Vector4f::Zero`、`doNotOptimize(trace)` | 工作与屏障 |
| 由坐标填充 `fa`/`fb` | 范数输入 |
| `doNotOptimize(nacc+mx.x+fmax[…])` | norms 之后的屏障 |
| `makePolygonFromFilteredExtent` | 本文件辅助 |
| `NormalEstimation`、`search::KdTree` | 法线 |
| `pcl::transformPointCloud` | 刚体 |
| `normals_t`：`R*n`、`normalize` | Eigen 标量环 |
| `rasterizeZToIntensityGrid`；强度→`zimg` 手写归一 | bench 栅格 |
| `rasterizeZToIntensityGrid`（→`img2d`） | 第二路栅格 |
| `pcl::copyPointCloud` | 准备 2d |
| `doNotOptimize(dists[0]…)` | 末段屏障 |

栅格与高斯、`zimg` 与后续 2d 耗时长，单次迭代远大于 hot。

---

## 3. 数据与参数

下表为三份应用在未指定外部 PCD 时的默认合成负载（可选用命令行传入 PCD 覆盖 raw 输入，以下为板卡日志中的实际配置）。


| 项目           | pipeline_app                                 | pipeline_hot                       | pipeline_dag                    |
| ------------ | -------------------------------------------- | ---------------------------------- | ------------------------------- |
| 原始点云         | 合成 50000 点 XYZ                               | 50000 点                            | 50000 点                         |
| 其它点云资产       | SAC 平面场景 10000 点；线段云 512 点（段内 getMaxSegment） | 线段云 512 点                          | 滤波后点数随体素（verbose 可查约 24998）     |
| 范数向量         | dim 128，每轮重复 400 × (L2+L1)                   | 同左                                 | DAG 内有独立 norms 小段（维度与循环与源码常量一致） |
| 与高斯 / 网格相关尺寸 | img2d 400×224；gauss_gray 480×272             | Gaussian 浮点图 480×272（Image Size 行） | 栅格 400×224                      |
| SAC          | 专用 10k 云                                     | 无                                  | 链末平面 + 法线平面                     |


Warmup / Iterations

- pipeline_app 与 pipeline_hot：Warmup 3，Iterations 12。
- pipeline_dag：Warmup 3，Iterations 10。

板卡实测时的附加选项

- pipeline 一侧日志带 `-v`，首次迭代在 stderr 打印阶段标记；hot/dag 当次记录未带 `-v`（可由 `BOARD_*_ARGS` 指定）。

---

## 4. 结果

以下数值直接来自三组 `output/board/*_compare.log` 中的表格与首尾打印。

### 4.1 `pipeline_compare.log` — 全链路


| 指标               | Std（`pipeline_app_std`） | RVV（`pipeline_app_rvv`） | 加速比     |
| ---------------- | ----------------------- | ----------------------- | ------- |
| 平均 `ms/iter`     | 1215.43                 | 1101.60                 | 约 1.10× |
| 近似总时间 `Avg × 12` | 14585.16 ms             | 13219.23 ms             | —       |


滤波后点数（`-v` 首次迭代）：24998。

### 4.2 `hot_compare.log` — 摘掉滤波 / KdTree / 整块 2d / SAC 等重头阶段后


| 指标           | Std       | RVV      | 加速比     |
| ------------ | --------- | -------- | ------- |
| 平均 `ms/iter` | 14.49     | 8.22     | 约 1.76× |
| `Avg × 12`   | 173.85 ms | 98.68 ms | —       |


### 4.3 `dag_compare.log` — 单链 DAG


| 指标           | Std          | RVV         | 加速比     |
| ------------ | ------------ | ----------- | ------- |
| 平均 `ms/iter` | 10023.52     | 2831.13     | 约 3.54× |
| `Avg × 10`   | 100235.19 ms | 28311.27 ms | —       |


---

## 5. 如何阅读上述差异

- 全链路 pipeline 倍率偏小（约 1.1×）：单次迭代很多时间落在 KdTree 邻域、体素滤波、变换与内存带宽以及 RANSAC 控制流上；其中 norms、[gaussian](../../common/src/gaussian.cpp)、[2d](../../2d/include/pcl/2d/impl/convolution.hpp)、SAC 部分 API 虽有 `__RVV10__` 路径，但整体仍受 Amdahl 与未向量化段落制约。
- hot 倍率中等（约 1.76×）：去掉滤波/整块 2d/RANSAC 后，[norms](../../common/include/pcl/common/impl/norms.hpp) 与高斯、`common` 统计及（可选）数学短条在计时中占比上升，故 `std/rvv` 反差优于全链路。
- DAG 倍率最高（约 3.54×）：板卡快照中单次迭代可达秒级；本地栅格化、`zimg` 上可分高斯、[2d](../../2d/include/pcl/2d/impl/convolution.hpp) 等大段规整算术占比高，可走 RVV 的算子更易主导反差；主要为 workload 形状差异，一般不表示 `pipeline_app` 实现有误。

若需要更细的模块对比，需在源码中为各阶段增加独立计时或借助板上 profiler。

---

## 6. 相关路径


| 内容     | 路径                                                                       |
| ------ | ------------------------------------------------------------------------ |
| 板卡快照日志 | `test-rvv/app/output/board/*.log`                                        |
| 源码     | `pcl_pipeline_app.cpp`、`pcl_pipeline_hot_rvv.cpp`、`pcl_pipeline_dag.cpp` |
| 开发与部署  | `test-rvv/app/Makefile`；板端运行示例 `test-rvv/app/board.mk`                   |
| 对比脚本   | `test-rvv/script/analyze_bench_compare.py`                               |


文档版本说明：内容与上述 `output/board` 快照一致时可视为对该次跑数的描述；若要作为正式报告存档，建议在刷新跑数后更新第 4 节表格数值或注明日期与 git 修订。
