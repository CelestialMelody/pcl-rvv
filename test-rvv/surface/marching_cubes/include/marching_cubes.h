/*
 * 本文件做什么：
 * 这是 marching_cubes RVV topic 的测试支撑聚合入口。测试和 bench 只 include
 * 这个稳定入口；具体 reference（参考链路）、fixture（测试输入）和 RVV candidate
 * 放在 include/impl/ 下，避免把内部 helper 直接暴露给测试入口。
 *
 * 证据边界：
 * 这些 helper 都是 test-only production-shaped diagnostic（测试专用生产形态诊断），
 * 复刻 createSurface 的主要语义，但不证明 production dispatch（生产分流）已经存在。
 */

#pragma once

#include "impl/marching_cubes_core.hpp"
