/*
 * 本文件只服务 test-rvv correctness：它让 gtest 编译 `image_depth.cpp` 时
 * 打开 path hit hook，用来证明真实 DepthImage public entry 命中 RVV 分流。
 * bench 仍直接链接无 hook 的 production cpp，避免污染 production-public 计时边界。
 */

#define PCL_RVV_IMAGE_DEPTH_TEST_HOOK
#include "../../../../io/src/image_depth.cpp"
