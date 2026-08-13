/*
 * 本文件做什么：
 * TEPTPL bench 的薄入口。实际 fixtures、component-only helper 和 case registry
 * 位于 include/impl/，这里仅把 main() 转发到稳定 bench harness。
 */

#include "bench_teptpl.h"

int
main(int argc, char** argv)
{
  return pcl::registration::rvv_te_pt2plane_lls_bench::run_teptpl_bench(argc, argv);
}
