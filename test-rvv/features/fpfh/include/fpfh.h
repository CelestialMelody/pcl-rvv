#pragma once

/*
 * FPFH RVV topic test-support aggregator（测试支撑聚合入口）。
 *
 * 本入口只服务 `test-rvv/features/fpfh` 下的 correctness（正确性）和
 * benchmark（性能测试）资产。它不修改 production（生产源码），也不能单独
 * 证明 `features/include/pcl/features/impl/fpfh.hpp` 已经接入 RVV dispatch
 * （分流逻辑）。
 */

#include "impl/fpfh_reference.hpp"
#include "impl/fpfh_weighted_candidate.hpp"
