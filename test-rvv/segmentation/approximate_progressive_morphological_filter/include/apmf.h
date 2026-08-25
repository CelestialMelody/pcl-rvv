#pragma once

/*
 * 本聚合头只服务 approximate_progressive_morphological_filter 的 RVV
 * diagnostic（诊断）测试资产。它不被 production（生产源码）包含，也不改变
 * PCL 公开入口；调用方通过这里进入同构标量 / RVV component helper。
 */

#include "impl/apmf_components.hpp"
