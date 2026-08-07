/*
 * 本文件做什么：
 * 这是 transformation_estimation_point_to_plane_lls_weighted 主题的稳定聚合入口。
 * 具体实现按 teptplw_common / teptplw_fixtures / teptplw_row_sources /
 * teptplw_reductions / teptplw_candidates 拆到 include/impl/，
 * 仍只用于 test-rvv diagnostic，不修改 PCL production。
 */

#pragma once

#include "impl/teptplw_candidates.hpp"
#include "impl/teptplw_fixtures.hpp"
