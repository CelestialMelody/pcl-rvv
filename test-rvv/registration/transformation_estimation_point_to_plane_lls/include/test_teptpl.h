/*
 * 本文件做什么：
 * 这是 TEPTPL gtest 源码使用的聚合入口。它在 topic 总入口基础上加入
 * gtest-only fixtures、assertions 和 production helper bridge，不给 bench 源码 include。
 */

#pragma once

#include "teptpl.h"
#include "impl/teptpl_test_helpers.hpp"
