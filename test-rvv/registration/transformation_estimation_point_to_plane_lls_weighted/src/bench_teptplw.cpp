/*
 * 本文件做什么：
 * transformation_estimation_point_to_plane_lls_weighted bench 的薄 main 入口。
 * 具体 case registry、计时边界和 trace 输出合同在 include/impl/teptplw_bench_cases.hpp。
 */

#include "bench_teptplw.h"

using namespace pcl::registration::rvv_te_pt2plane_lls_weighted_bench;

int
main(int argc, char** argv)
{
  const BenchOptions options = parse_options(argc, argv);
  set_warmup_iterations(options.warmup_iterations);
  print_bench_header(options);
  print_results(collect_bench_results(options));
  return 0;
}
