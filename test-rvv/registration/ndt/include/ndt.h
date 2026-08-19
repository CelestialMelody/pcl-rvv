/*
 * 本文件做什么：
 * ndt topic 的稳定测试聚合入口。test 和 bench 只 include 这个文件，
 * 具体 fixture（输入样本）、reference（标量参考链路）和 candidate
 *（RVV 诊断候选）拆在 include/impl 下。
 *
 * 证据边界：
 * 这些 helper 只服务 test-rvv 诊断，不修改
 * registration/include/pcl/registration/impl/ndt.hpp，也不证明
 * NormalDistributionsTransform public entry（公开入口）已经接入 RVV。
 * 当前候选只覆盖 computeDerivatives 内部 updateDerivatives 的分阶段暂存
 * derivative accumulation（导数累加）数学核，不覆盖 voxel neighbor search
 *（体素邻域搜索）、computePointDerivatives（点导数预计算）或 Eigen solver
 *（求解器）。
 */

#pragma once

#include "impl/ndt_fixtures.hpp"
#include "impl/ndt_references.hpp"
#include "impl/ndt_candidates.hpp"
