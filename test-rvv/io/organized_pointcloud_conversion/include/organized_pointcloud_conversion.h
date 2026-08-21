/*
 * 本文件做什么：
 * 这是 organized_pointcloud_conversion topic 的测试支撑聚合入口。测试和 bench
 * 只 include 这一层，再由它暴露 fixture（输入构造）、candidate（候选实现）和
 * assertion/checksum（断言与校验）helper。
 *
 * 证据边界：
 * 这里的 helper 都是 test-only diagnostic（测试专用诊断）资产，不参与 production
 * dispatch（生产分流），不能单独证明真实生产入口已经接入 RVV。
 */

#pragma once

#include "impl/opc_candidates.hpp"
#include "impl/opc_fixtures.hpp"
#include "impl/opc_types.hpp"
