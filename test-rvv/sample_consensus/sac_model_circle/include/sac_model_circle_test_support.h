/*
 * 本文件做什么：
 * 这是 sac_model_circle RVV topic 的测试支撑聚合入口。test 和 bench 只 include
 * 这个头文件，内部候选实现放在 include/impl/ 下，避免测试源和 bench 源复制
 * 同一份 production-shaped diagnostic（生产形态诊断）逻辑。
 */

#pragma once

#include "impl/sac_model_circle_candidates.hpp"
