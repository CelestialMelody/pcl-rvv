/*
 * 本文件做什么：
 * 这是 pcd_io topic 的测试支撑聚合入口。测试和 bench 只通过这个稳定头
 * 访问 test-only（仅测试使用）reference / candidate helper；生产源码不会
 * include 本头文件。
 */

#pragma once

#include "impl/pcd_io_support.hpp"
