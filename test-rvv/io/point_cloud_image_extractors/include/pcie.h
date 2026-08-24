/*
 * 本文件做什么：
 * 这是 point_cloud_image_extractors topic 的稳定聚合入口。测试和 bench 只
 * include 这个头；具体 fixtures（测试输入）、reference（标量参考）和
 * candidates（候选实现）放在 include/impl 中，避免入口源文件直接依赖内部
 * 拆分细节。
 *
 * 证据边界：
 * 这里的 helper 都是 test support（测试支撑代码），不能证明 production
 * dispatch（生产分流）已经接入 RVV。
 */

#pragma once

#include "impl/pcie_support.hpp"
