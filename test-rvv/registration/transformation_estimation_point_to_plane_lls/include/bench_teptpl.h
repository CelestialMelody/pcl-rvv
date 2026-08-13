/*
 * 本文件做什么：
 * 这是 TEPTPL bench 源码使用的聚合入口。它包含无 gtest 依赖的 fixtures、
 * component helper 和 bench case registry，保持 src/bench_teptpl.cpp 为薄入口。
 */

#pragma once

#include "teptpl.h"
#include "impl/teptpl_bench_cases.hpp"
