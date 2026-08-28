/*
 * 本文件做什么：
 * 这是 sac_model_cylinder RVV topic 的测试支撑聚合入口。测试和 bench
 * 只 include 这一层；内部 helper 放在 include/impl/ 下。当前阶段已经有
 * count/select 的测试专用 candidate helper，后续 bench 和 Evidence Doctor
 * 都从这个聚合入口恢复。
 */

#pragma once

#include "impl/sac_model_cylinder_diagnostic.hpp"
