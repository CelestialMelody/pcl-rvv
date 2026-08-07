/*
 * 本文件做什么：
 * weighted point-to-plane LLS diagnostic 的 candidate 聚合入口。具体实现按
 * full-cloud、row source 和 estimate wrapper 拆分到窄职责内部头。
 */

#pragma once

#include "teptplw_candidate_estimates.hpp"
