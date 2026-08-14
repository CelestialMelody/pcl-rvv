/*
 * 本文件做什么：
 * 这是 ICP topic 的 test-rvv support 聚合入口。测试和 bench 只 include 这个稳定入口，
 * 内部 helper 放在 `include/impl/`，避免把候选实现和测试入口混在一个长文件里。
 *
 * 证据边界：
 * 这里是 test-only diagnostic（测试专用诊断）代码，不是 production dispatch（生产分流）。
 * 它只能证明 `IterativeClosestPoint::transformCloud` 形状的候选是否值得继续接入生产。
 */

#pragma once

#include "impl/icp_transform_cloud.hpp"
