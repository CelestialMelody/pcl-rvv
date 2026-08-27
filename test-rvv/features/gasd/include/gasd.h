/*
 * 本聚合头（aggregator header）是 features/gasd topic 的稳定测试入口。
 * 具体 reference（参考链路）和 candidate（候选实现）放在 include/impl，
 * 测试和 bench 只依赖这一层。
 */

#pragma once

#include "impl/gasd_copy_candidate.hpp"
#include "impl/gasd_reference.hpp"
