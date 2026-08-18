/*
 * 本文件做什么：
 * bfgs topic 的稳定测试聚合入口。test 和 bench 只 include 这个文件，
 * 内部 helper 按 fixtures（输入样本）、references（标量参考链路）和
 * candidates（诊断候选）拆在 include/impl 下。
 *
 * 证据边界：
 * 这些 helper 只服务 test-rvv 诊断，不修改
 * registration/include/pcl/registration/bfgs.h，也不证明 production
 * dispatch（生产分流）已经存在。
 */

#pragma once

#include <pcl/registration/bfgs.h>

#include "impl/bfgs_fixtures.hpp"
#include "impl/bfgs_references.hpp"
#include "impl/bfgs_candidates.hpp"
