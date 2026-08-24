/*
 * 本聚合头（aggregator header）是 pcd_io_templated_writer topic 的稳定测试入口。
 * 内部 helper 放在 include/impl，避免测试和 bench 直接依赖多个实现细节头。
 */

#pragma once

#include "impl/pcdtw_support.hpp"
