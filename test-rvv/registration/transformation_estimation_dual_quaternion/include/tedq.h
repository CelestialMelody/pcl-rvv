/*
 * 本文件做什么：
 * 这是 transformation_estimation_dual_quaternion 的稳定测试支撑聚合入口。
 * test / bench 源码只 include 这个文件；具体 reference（参考链路）和 RVV candidate
 * （RVV 候选链路）放在 include/impl/tedq_candidates.hpp。
 *
 * 主题短标识：
 * transformation_estimation_dual_quaternion -> tedq。使用短标识是为了避免
 * src/test_*.cpp、src/bench_*.cpp 和内部头文件名过长。
 */

#pragma once

#include "impl/tedq_candidates.hpp"
