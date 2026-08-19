/*
 * 本文件做什么：
 * 这是 transformation_estimation_2D 的 test-rvv support 聚合入口。`te2d` 是
 * transformation_estimation_2D 的短标识，测试和 bench 只 include 这个稳定入口。
 *
 * 证据边界：
 * 这里聚合的是测试专用诊断代码，不是 production dispatch（生产分流）。当前
 * ordered-cloud-pair 诊断已扩展到代表性 PointXYZ-like 点型和 mixed source/target
 * 组合，但 production patch 仍保留窄范围 exact `PointXYZ -> PointXYZ` /
 * `Scalar=float` 行为，等待后续证据闭合。
 */

#pragma once

#include "impl/te2d_candidates.hpp"
