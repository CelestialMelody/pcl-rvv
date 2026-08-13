/*
 * 本文件做什么：
 * 这是 TEPTPL（transformation_estimation_point_to_plane_lls 的短标识）
 * test-rvv support 聚合入口。测试和 bench 专用入口会先 include 这个稳定入口，
 * 再按各自需要加入 gtest-only 或 bench-only helper；具体 reference、RVV math、
 * row source、reduction 和 candidate helper 位于 `include/impl/` 内部分层。
 *
 * 证据边界：
 * 这里聚合的是测试支撑代码，不是 production dispatch。它不能把 full-cloud
 * production candidate 外推到 indexed、correspondences、weighted 或 `Scalar=double`。
 */

#pragma once

#include "impl/teptpl_candidates.hpp"
