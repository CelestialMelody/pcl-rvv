/*
 * 本文件做什么：
 * 这是 sac_model_circle3d RVV topic 的测试支撑聚合入口。test 和 bench
 * 只 include 这个头文件；内部候选实现放在 include/impl/ 下。这里的 helper
 * 都是 test-only（仅测试使用）诊断代码，不能证明 production dispatch（生产分流）
 * 已经存在。
 */

#pragma once

#include "impl/sac_model_circle3d_candidates.hpp"
