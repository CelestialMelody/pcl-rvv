#pragma once

/** 单次迭代「分阶段耗时」：`StageAccumulator::printSummary` 的 stdout 标签。
 *  格式：`[PIPnn] 阶段一句描述 —` 换行后再 `common/…`、`filters/` 等分段列函数；续行 8 空格缩进。
 *  不写 `pcl::`；`common/common` 对应 common.h / common/impl/common.hpp；RVV 初等同列于此。
 *  `-v` stderr 为人读短句（见各 pipeline*.cpp），不求与本字符串逐字相同。
 *  权威说明：`board-benchmark-report.zh.md` §2。 */

namespace pcl_test_rvv_app {

// --- pipeline_compare / pcl_pipeline_app.cpp（record 索引 0..6） ---
inline constexpr const char* kBenchStagePIP01 =
    "[PIP01] raw 点云：common 全段（含 RVV 初等条带）—\n"
    "        common/centroid: compute3DCentroid, computeMeanAndCovarianceMatrix;\n"
    "        common/common: getMinMax3D, getMaxDistance;\n"
    "        common/norms: L2_Norm, L1_Norm;\n"
    "        common/common: getMaxSegment;\n"
    "        common/gaussian: GaussianKernel::convolveRows, convolveCols (gauss_gray);\n"
    "        common/common: calculatePolygonArea;\n"
    "        common/common: expf_RVV_f32m2, logf_RVV_f32m2, atan2_RVV_f32m2, acos_RVV_f32m2";

inline constexpr const char* kBenchStagePIP02 =
    "[PIP02] 标量占位 —\n"
    "        common/common: getAngle3D";

inline constexpr const char* kBenchStagePIP03 =
    "[PIP03] 体素滤波 —\n"
    "        filters: VoxelGrid::filter; copyPointCloud (sparse fallback)";

inline constexpr const char* kBenchStagePIP04 =
    "[PIP04] 半径法线 + 邻域 —\n"
    "        features: NormalEstimation::compute; search/KdTree (radius normals)";

inline constexpr const char* kBenchStagePIP05 =
    "[PIP05] 刚体变换 —\n"
    "        transforms: transformPointCloud (rigid on filtered)";

inline constexpr const char* kBenchStagePIP06 =
    "[PIP06] 预制强度图上的 2d —\n"
    "        2d: copyPointCloud(img2d_master); Convolution::filter;\n"
    "        Morphology::erosionGray; Edge::detectEdgeSobel";

inline constexpr const char* kBenchStagePIP07 =
    "[PIP07] 专用 SAC 点云 —\n"
    "        sample_consensus (cloud_sac): SampleConsensusModelPlane::countWithinDistance;\n"
    "        SampleConsensusModelNormalPlane::selectWithinDistance,\n"
    "        countWithinDistance, getDistancesToModel";

// --- pipeline_hot / pcl_pipeline_hot_rvv.cpp（record 索引 0..1） ---
inline constexpr const char* kBenchStageHOT01 =
    "[HOT01] raw common 热区（无 voxel/法线/tf/2d/SAC）—\n"
    "        common/centroid: compute3DCentroid, computeMeanAndCovarianceMatrix;\n"
    "        common/common: getMinMax3D, getMaxDistance;\n"
    "        common/norms: L2_Norm, L1_Norm (norm_repeat);\n"
    "        common/common: getMaxSegment;\n"
    "        common/gaussian: GaussianKernel::convolveRows, convolveCols;\n"
    "        common/common: calculatePolygonArea";

inline constexpr const char* kBenchStageHOT02 =
    "[HOT02] RVV 初等条带（仅 __RVV10__）—\n"
    "        common/common: expf_RVV_f32m2, logf_RVV_f32m2,\n"
    "        atan2_RVV_f32m2, acos_RVV_f32m2";

// --- pipeline_dag / pcl_pipeline_dag.cpp（record 索引 0..7） ---
inline constexpr const char* kBenchStageDAG01 =
    "[DAG01] 体素滤波 —\n"
    "        filters: VoxelGrid::filter; copyPointCloud (sparse fallback)";

inline constexpr const char* kBenchStageDAG02 =
    "[DAG02] 滤波后点云 cloud_f 上 common —\n"
    "        common/centroid: compute3DCentroid, computeMeanAndCovarianceMatrix;\n"
    "        common/common: getMinMax3D, getMaxDistance, getMaxSegment;\n"
    "        common/norms: L2_Norm, L1_Norm (x32);\n"
    "        bench: makePolygonFromFilteredExtent; common/common: calculatePolygonArea";

inline constexpr const char* kBenchStageDAG03 =
    "[DAG03] RVV 初等条带（仅 __RVV10__ 且 scratch 非空）—\n"
    "        common/common: expf_RVV_f32m2, logf_RVV_f32m2,\n"
    "        atan2_RVV_f32m2, acos_RVV_f32m2";

inline constexpr const char* kBenchStageDAG04 =
    "[DAG04] 半径法线 + 邻域 —\n"
    "        features: NormalEstimation::compute; search/KdTree (cloud_f)";

inline constexpr const char* kBenchStageDAG05 =
    "[DAG05] 变换点云并旋转法线 —\n"
    "        transforms: transformPointCloud; Eigen rotate+normalize normals\n"
    "        (normals_f -> normals_t)";

inline constexpr const char* kBenchStageDAG06 =
    "[DAG06] Z 栅格 + 可分高斯 —\n"
    "        bench: rasterizeZToIntensityGrid(cloud_t)->zimg;\n"
    "        common/gaussian: GaussianKernel::convolveRows, convolveCols";

inline constexpr const char* kBenchStageDAG07 =
    "[DAG07] 强度栅格 + pcl::2d —\n"
    "        bench: rasterizeZToIntensityGrid(cloud_t)->img2d;\n"
    "        2d: copyPointCloud; Convolution::filter; Morphology::erosionGray;\n"
    "        Edge::detectEdgeSobel";

inline constexpr const char* kBenchStageDAG08 =
    "[DAG08] 变换链末端 SAC（每迭代构造模型）—\n"
    "        sample_consensus: per-iter SampleConsensusModelPlane,\n"
    "        SampleConsensusModelNormalPlane; countWithinDistance,\n"
    "        selectWithinDistance, getDistancesToModel (cloud_t, normals_t)";

} // namespace pcl_test_rvv_app
