/*
 * sac_model_sphere bench 的源码入口。
 * bench harness 和点型选择实现放在 include/，这里保留 CLI 解析和错误返回合同。
 */

#include <bench_sac_model_sphere.h>

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>

using namespace pcl_rvv_sphere_test_support;

int
main (int argc, char** argv)
{
  const std::size_t nr_points = (argc >= 2) ? std::max<std::size_t> (1, std::strtoull (argv[1], nullptr, 10)) : 65536;
  const int iterations = (argc >= 3) ? std::max (1, std::atoi (argv[2])) : 200;
  const std::string point_type = (argc >= 4) ? argv[3] : "PointXYZ";

  if (point_type == "PointXYZ")
    return runBenchForPointType<pcl::PointXYZ> ("PointXYZ", nr_points, iterations);
  if (point_type == "PointXYZI")
    return runBenchForPointType<pcl::PointXYZI> ("PointXYZI", nr_points, iterations);
  if (point_type == "PointXYZRGB")
    return runBenchForPointType<pcl::PointXYZRGB> ("PointXYZRGB", nr_points, iterations);
  if (point_type == "PointXYZRGBA")
    return runBenchForPointType<pcl::PointXYZRGBA> ("PointXYZRGBA", nr_points, iterations);

  std::cerr << "Unsupported point type: " << point_type
            << " (expected PointXYZ, PointXYZI, PointXYZRGB or PointXYZRGBA)\n";
  return 2;
}
