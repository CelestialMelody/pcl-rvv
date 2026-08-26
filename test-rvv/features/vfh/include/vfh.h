#pragma once

/*
 * 本聚合头只服务 VFH（Viewpoint Feature Histogram，视点特征直方图）的
 * RVV diagnostic（诊断）测试资产。它不被 production（生产源码）包含，
 * 也不改变 PCL 公开入口；调用方通过这里进入同构标量 reference
 * （参考链路）和 test-only candidate（测试专用候选）。
 */

#include "impl/vfh_reference.hpp"
