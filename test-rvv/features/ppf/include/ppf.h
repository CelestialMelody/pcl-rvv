#pragma once

/*
 * PPF RVV topic test-support aggregator（测试支撑聚合入口）。
 *
 * 本入口只服务 `test-rvv/features/ppf` 下的 correctness（正确性）和后续
 * benchmark（性能测试）资产。它不证明 `features/include/pcl/features/impl/ppf.hpp`
 * 已经接入 production RVV dispatch（生产 RVV 分流）。
 */

namespace pcl::features::rvv_test::ppf
{
}

#include "impl/ppf_reference.hpp"
#include "impl/ppf_alpha_candidate.hpp"
#include "impl/ppf_pair_batch_candidate.hpp"
