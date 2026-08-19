/*
 * 本文件做什么：
 * gicp topic 的稳定测试聚合入口。test 和 bench 只 include 这个文件，
 * 内部 helper 按 fixtures（输入样本）、references（标量参考链路）和
 * candidates（RVV 诊断候选）拆在 include/impl 下。
 *
 * 证据边界：
 * 这些 helper 只服务 test-rvv 诊断，不修改
 * registration/include/pcl/registration/impl/gicp.hpp，也不证明
 * production dispatch（生产分流）已经存在。当前候选只覆盖 GICP
 * residual / Mahalanobis 累加和 covariance post-KNN（KNN 之后的协方差）
 * 局部组件。
 */

#pragma once

#include "impl/gicp_fixtures.hpp"
#include "impl/gicp_references.hpp"
#include "impl/gicp_candidates.hpp"
