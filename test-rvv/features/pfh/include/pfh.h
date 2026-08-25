#pragma once

/*
 * PFH RVV topic test-support aggregator（测试支撑聚合入口）。
 *
 * 本入口只服务 `test-rvv/features/pfh` 下的 correctness（正确性）和
 * benchmark（性能测试）资产。它不证明 `features/include/pcl/features/impl/pfh.hpp`
 * 已经接入 production RVV dispatch（生产 RVV 分流）。
 */

namespace pcl::features::rvv_test::pfh
{
}

#include "impl/pfh_reference.hpp"
#include "impl/pfh_pair_batch_candidate.hpp"
