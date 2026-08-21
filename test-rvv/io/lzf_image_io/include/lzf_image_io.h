#pragma once

/*
 * 本文件做什么：
 * 这里放 lzf_image_io topic 的测试专用 reference（参考链路）和 RVV
 * candidate（候选链路）。它复刻 `LZF*ImageReader` 解压后转换循环，
 * 供 gtest 和 bench 对拍。
 *
 * 证据边界：
 * 本文件不属于 production（生产源码），也不改变 PCLZF reader 的公开
 * 行为。`__RVV10__` 关闭时 candidate 自然回到同一份标量参考链路。
 */

#include <cstddef>
#include <cstdint>

#include "impl/lzf_image_io_support.hpp"
