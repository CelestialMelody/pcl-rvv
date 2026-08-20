/*
 * 本文件做什么：
 * 这是 marching_cubes_rbf topic 的测试支撑聚合入口。测试和 bench 通过这里
 * 访问同一组 RBF（radial basis function，径向基函数）组件消融 helper。
 *
 * 证据边界：
 * 这些 helper 是 test-rvv 下的 diagnostic（诊断）资产，不修改 production
 * 源码，也不证明真实 production dispatch（生产分流）已经接入 RVV。
 */

#pragma once

#include "impl/marching_cubes_rbf_core.hpp"
