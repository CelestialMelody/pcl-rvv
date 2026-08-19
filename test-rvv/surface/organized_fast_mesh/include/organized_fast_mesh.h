/*
 * 本文件是 organized_fast_mesh RVV topic 的稳定测试支撑入口。
 * 测试和 bench 只 include 这一层，再由它引入内部 helper；这些 helper
 * 都是 test-rvv 诊断资产，不能证明 production dispatch（生产分流）已经存在。
 */

#pragma once

#include "impl/ofm_candidates.hpp"
#include "impl/ofm_fixtures.hpp"
#include "impl/ofm_reference.hpp"
#include "impl/ofm_types.hpp"
