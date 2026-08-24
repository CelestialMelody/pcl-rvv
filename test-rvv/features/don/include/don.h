/*
 * 本聚合头（aggregator header）是 features/don topic 的稳定测试入口。
 * 具体 reference（参考链路）、candidate（候选实现）和 bench helper 放在
 * include/impl，测试和 bench 只依赖这一层，方便 reviewer 定位边界。
 */

#pragma once

#include "impl/don_core.hpp"
