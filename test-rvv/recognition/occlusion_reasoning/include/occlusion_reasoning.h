#pragma once

/*
 * 本文件做什么：
 * 这是 recognition/occlusion_reasoning RVV topic 的测试支撑聚合入口。
 * 当前只覆盖 ZBuffering::filter() 的 projection（投影）、depth compare
 *（深度比较）和 keep indices（保留索引）诊断链路。
 *
 * 证据边界：
 * 这里的 helper 只属于 test-rvv（专项测试资产），不被 production（生产源码）
 * 包含，也不能证明真实 production dispatch（生产分流）已经接入 RVV。
 */

#include "impl/occlusion_reasoning_candidates.hpp"
