/*
 * 本文件做什么：
 * ICP transformCloud bench 的薄入口。QEMU 运行只用于构建、checksum（校验和）和日志形状；
 * 性能结论必须来自板卡或目标硬件 repeated benchmark。
 */

#include "bench_icp.h"

int
main()
{
  return pcl::registration::rvv_icp_bench::run_icp_bench();
}
