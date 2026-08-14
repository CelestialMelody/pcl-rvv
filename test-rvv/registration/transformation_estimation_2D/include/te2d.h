/*
 * 本文件做什么：
 * 这是 transformation_estimation_2D 的 test-rvv support 聚合入口。`te2d` 是
 * transformation_estimation_2D 的短标识，测试和 bench 只 include 这个稳定入口。
 *
 * 证据边界：
 * 这里聚合的是测试专用诊断代码，不是 production dispatch（生产分流）。当前 candidate
 * 只覆盖顺序点云对（ordered-cloud-pair，source/target 按相同下标一一对应）的
 * dense finite `PointXYZ -> PointXYZ` / `Scalar=float` 输入。
 */

#pragma once

#include "impl/te2d_candidates.hpp"
