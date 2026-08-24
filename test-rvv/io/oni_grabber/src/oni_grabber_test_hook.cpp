/*
 * 本文件只服务 test-rvv correctness（正确性测试）：它让 gtest 编译
 * `oni_grabber.cpp` 时打开 production detail helper（生产内部 helper）
 * hook，用来证明测试直接调用的是生产源码里的 depth-only 填充逻辑。
 */

#define PCL_RVV_ONI_GRABBER_TEST_HOOK
#include "../../../../io/src/oni_grabber.cpp"
