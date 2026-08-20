/*
 * 本文件做什么：
 * 这是 transformation_estimation_2D 的 test-rvv support 聚合入口。`te2d` 是
 * transformation_estimation_2D 的短标识，测试和 bench 只 include 这个稳定入口。
 *
 * 证据边界：
 * 这里聚合的是测试专用诊断代码，不是 production dispatch（生产分流）。内部
 * helper 已按职责拆到 include/impl/te2d_*.hpp；历史单一实现入口不再保留。
 */

#pragma once

#include "impl/te2d_core_types.hpp"
#include "impl/te2d_fixtures.hpp"
#include "impl/te2d_layout_helpers.hpp"
#include "impl/te2d_row_sources.hpp"
#include "impl/te2d_public_wrappers.hpp"
#include "impl/te2d_family_ab.hpp"
#include "impl/te2d_ordered_candidates.hpp"
#include "impl/te2d_source_indexed_candidates.hpp"
#include "impl/te2d_dual_indexed_candidates.hpp"
#include "impl/te2d_correspondence_candidates.hpp"
#include "impl/te2d_checksums.hpp"
