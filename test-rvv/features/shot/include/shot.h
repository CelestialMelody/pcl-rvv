/*
 * SHOT RVV topic test-support aggregator.
 *
 * 本聚合头只服务 test-rvv/features/shot 的测试和 bench（性能测试）。它提供
 * 合成输入、固定 reference frame（局部参考系）和 descriptor（描述子）检查 helper，
 * 不能证明 production dispatch（生产分流）已经接入 RVV。
 */

#pragma once

#include "impl/shot_color.hpp"
#include "impl/shot_fixtures.hpp"
#include "impl/shot_interpolate.hpp"
#include "impl/shot_normalize.hpp"
#include "impl/shot_shape_bin.hpp"
