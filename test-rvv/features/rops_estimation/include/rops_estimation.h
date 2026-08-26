/*
 * 本聚合头（aggregator header）是 features/rops_estimation topic 的稳定
 * test-rvv 入口。内部 reference（参考链路）和 candidate（候选实现）会放在
 * include/impl；测试和 bench 只依赖这一层，避免直接耦合临时文件布局。
 */

#pragma once

#include "impl/rops_components.hpp"
