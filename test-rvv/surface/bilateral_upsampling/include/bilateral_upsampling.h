#pragma once

/*
 * 本聚合头是 test-rvv/surface/bilateral_upsampling 的稳定入口。
 * 这里的代码只服务 topic-local reference、diagnostic、bench 和 production
 * direct correctness 对照；真实生产实现仍在 surface/include/pcl/surface/impl/
 * bilateral_upsampling.hpp 中维护。
 */

#include "impl/bilateral_upsampling_core.hpp"
