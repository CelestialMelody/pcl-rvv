/*
 * 本文件做什么：
 * 这是 transformation_estimation_point_to_plane_lls_weighted gtest 源码使用的聚合入口。
 * 它在 topic 总入口基础上加入 gtest-only assertions，不给 bench 源码 include。
 */

#pragma once

#include "teptplw.h"

#include "impl/teptplw_assertions.hpp"
#include "impl/teptplw_test_helpers.hpp"
