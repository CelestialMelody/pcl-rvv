#pragma once

/*
 * CPPF RVV topic test-support aggregator（测试支撑聚合入口）。
 *
 * 本入口只服务 `test-rvv/features/cppf` 下的 correctness（正确性）和
 * benchmark（性能测试）资产。这里的 RVV candidate（候选实现）是 test-only
 * component ablation（测试专用组件消融），不证明 production dispatch（生产分流）。
 */

namespace pcl::features::rvv_test::cppf
{
}

#include "impl/cppf_reference.hpp"
#include "impl/cppf_alpha_candidate.hpp"
#include "impl/cppf_pair_hsv_candidate.hpp"
