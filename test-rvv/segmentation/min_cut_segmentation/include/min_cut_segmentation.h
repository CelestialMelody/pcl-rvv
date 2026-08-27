#pragma once

/*
 * 本聚合头只服务 min_cut_segmentation 的 RVV diagnostic（诊断）测试资产。
 * 它不被 production（生产源码）包含，也不改变 PCL 公开入口；调用方通过这里
 * 进入同构标量 / RVV component helper（组件辅助函数）。
 */

#include "impl/min_cut_segmentation_components.hpp"
