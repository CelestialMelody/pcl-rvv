#include "transformation_validation_euclidean_diag.hpp"

// 本文件做什么：
// 这个 benchmark（性能测试）把 TransformationValidationEuclidean 的 test-only
// 诊断拆成四类计时边界：transform staging、KdTree setup、search-only negative
// control（只测最近邻搜索的负向对照）和 full validation。QEMU 运行只用于
// 构建、checksum 和日志形状；真实性能结论必须来自板卡或目标硬件。

#include <chrono>
#include <cmath>
#include <cstdint>
#include <functional>
#include <iomanip>
#include <iostream>
#include <string>

namespace diag = pcl::registration::transformation_validation_euclidean_diag;

namespace {

constexpr int kIterations = 20;
constexpr std::size_t kBannerWidth = 104;

void
printBanner(char ch)
{
  std::cout << std::string(kBannerWidth, ch) << '\n';
}

template <typename T>
inline void
doNotOptimize(const T& value)
{
#if defined(__GNUC__) || defined(__clang__)
  asm volatile("" : : "r,m"(value) : "memory");
#else
  (void)value;
#endif
}

class Benchmarker {
public:
  explicit Benchmarker(std::string name) : name_(std::move(name)) {}

  void run(const std::function<void()>& func, int iterations = kIterations, int warmup = 3) const
  {
    for (int i = 0; i < warmup; ++i)
      func();

    const auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < iterations; ++i)
      func();
    const auto stop = std::chrono::steady_clock::now();
    const double total_ms = std::chrono::duration<double, std::milli>(stop - start).count();
    std::cout << std::left << std::setw(72) << name_ << ": " << std::fixed
              << std::setprecision(4) << (total_ms / iterations) << " ms/iter\n";
    std::cout << "  Total Time: " << std::fixed << std::setprecision(4) << total_ms
              << " ms, checksum: " << checksum_ << '\n';
  }

  void setChecksum(std::uint64_t checksum) const { checksum_ = checksum; }

private:
  std::string name_;
  mutable std::uint64_t checksum_{0};
};

pcl::PointCloud<pcl::PointXYZ>::Ptr
makeCloud(std::size_t n)
{
  auto cloud = pcl::make_shared<pcl::PointCloud<pcl::PointXYZ>>();
  cloud->width = static_cast<std::uint32_t>(n);
  cloud->height = 1;
  cloud->is_dense = true;
  cloud->points.resize(n);
  for (std::size_t i = 0; i < n; ++i) {
    (*cloud)[i].x = static_cast<float>(static_cast<int>(i % 4099) - 2049) / 200.0f;
    (*cloud)[i].y = static_cast<float>(static_cast<int>((i * 7) % 4093) - 2046) / 220.0f;
    (*cloud)[i].z = static_cast<float>(static_cast<int>((i * 13) % 4091) - 2045) / 240.0f;
  }
  return cloud;
}

diag::Matrix4f
makeTransform()
{
  diag::Matrix4f t = diag::Matrix4f::Identity();
  t(0, 0) = 0.9848077f;
  t(0, 1) = -0.1736482f;
  t(1, 0) = 0.1736482f;
  t(1, 1) = 0.9848077f;
  t(0, 3) = 0.075f;
  t(1, 3) = -0.045f;
  t(2, 3) = 0.030f;
  return t;
}

std::uint64_t
checksumCloud(const pcl::PointCloud<pcl::PointXYZ>& cloud)
{
  std::uint64_t hash = 1469598103934665603ull;
  for (std::size_t i = 0; i < cloud.size(); i += 17) {
    const auto& p = cloud[i];
    const auto bucket = static_cast<std::int64_t>(
        std::llround((p.x * 3.0f + p.y * 5.0f + p.z * 7.0f) * 100000.0f));
    hash ^= static_cast<std::uint64_t>(bucket);
    hash *= 1099511628211ull;
  }
  return hash ^ cloud.size();
}

std::uint64_t
checksumScore(double score)
{
  return static_cast<std::uint64_t>(std::llround(score * 1000000000.0));
}

void
benchTransform(const std::string& name, const pcl::PointCloud<pcl::PointXYZ>& source)
{
  const auto transform = makeTransform();
  pcl::PointCloud<pcl::PointXYZ> transformed;
  Benchmarker bench(name);
  bench.run([&]() {
    diag::transformPointXYZCandidate(source, transformed, transform);
    bench.setChecksum(checksumCloud(transformed));
    doNotOptimize(transformed);
  });
}

// 只测 target KdTree setup。它回答“force_no_recompute/tree reuse 能省掉多少
// 一次性准备成本”，不包含 transform staging 或 nearestKSearch。
void
benchTreeSetup(const std::string& name,
               const pcl::PointCloud<pcl::PointXYZ>::ConstPtr& target)
{
  Benchmarker bench(name);
  bench.run([&]() {
    diag::KdTree tree;
    diag::setupTargetTree(tree, target);
    bench.setChecksum(target->size());
    doNotOptimize(&tree);
  });
}

// search-only negative control（负向对照）把 transformed cloud 和 KdTree 都提前准备好。
// std/RVV 两个构建理论上走同一条标量 search tail，因此 speedup 应接近 1x。
void
benchSearchOnly(const std::string& name,
                const pcl::PointCloud<pcl::PointXYZ>::ConstPtr& source)
{
  const auto transform = makeTransform();
  auto target = pcl::make_shared<pcl::PointCloud<pcl::PointXYZ>>();
  pcl::PointCloud<pcl::PointXYZ> transformed;
  diag::transformPointXYZStd(*source, *target, transform);
  diag::transformPointXYZStd(*source, transformed, transform);

  diag::KdTree tree;
  diag::setupTargetTree(tree, target);
  Benchmarker bench(name);
  bench.run([&]() {
    const auto result = diag::scoreTransformedWithTree(transformed, tree, 1.0);
    bench.setChecksum(checksumScore(result.score) ^
                      static_cast<std::uint64_t>(result.accepted_points));
    doNotOptimize(result.score);
  });
}

// fresh-tree full validation 对应 production 默认形态：每次计分都设置 target tree，
// 然后对 transformed source 逐点 nearestKSearch。
void
benchFullValidation(const std::string& name, const pcl::PointCloud<pcl::PointXYZ>::ConstPtr& source)
{
  const auto transform = makeTransform();
  auto target = pcl::make_shared<pcl::PointCloud<pcl::PointXYZ>>();
  diag::transformPointXYZStd(*source, *target, transform);
  Benchmarker bench(name);
  bench.run([&]() {
    const double score = diag::validateTransformationCandidate(source, target, transform, 1.0);
    bench.setChecksum(checksumScore(score));
    doNotOptimize(score);
  });
}

// prebuilt-tree full validation 对应 force_no_recompute/tree reuse 场景。它跳过
// KdTree setup，但仍保留 transform staging、nearestKSearch 和 score tail。
void
benchFullValidationPrebuiltTree(const std::string& name,
                                const pcl::PointCloud<pcl::PointXYZ>::ConstPtr& source)
{
  const auto transform = makeTransform();
  auto target = pcl::make_shared<pcl::PointCloud<pcl::PointXYZ>>();
  diag::transformPointXYZStd(*source, *target, transform);
  diag::KdTree tree;
  diag::setupTargetTree(tree, target);
  Benchmarker bench(name);
  bench.run([&]() {
    const auto result =
        diag::validateTransformationCandidateWithTree(source, tree, transform, 1.0);
    bench.setChecksum(checksumScore(result.score) ^
                      static_cast<std::uint64_t>(result.accepted_points));
    doNotOptimize(result.score);
  });
}

} // namespace

int
main()
{
  printBanner('=');
  std::cout << "PCL registration/transformation_validation_euclidean diagnostic benchmark\n";
#if defined(__RVV10__)
  std::cout << "Build: RVV (__RVV10__ enabled)\n";
#else
  std::cout << "Build: Std (__RVV10__ disabled)\n";
#endif
  std::cout << "Dataset: synthetic PointXYZ clouds; transform staging microbench and full KdTree validation diagnostic\n";
  std::cout << "Iterations: " << kIterations << '\n';
  printBanner('-');

  const auto source_64k = makeCloud(64 * 1024);
  const auto source_256k = makeCloud(256 * 1024);
  auto target_64k = pcl::make_shared<pcl::PointCloud<pcl::PointXYZ>>();
  auto target_256k = pcl::make_shared<pcl::PointCloud<pcl::PointXYZ>>();
  const auto transform = makeTransform();
  diag::transformPointXYZStd(*source_64k, *target_64k, transform);
  diag::transformPointXYZStd(*source_256k, *target_256k, transform);

  benchTransform("tve transform-staging pointxyz 64K", *source_64k);
  benchTransform("tve transform-staging pointxyz 256K", *source_256k);
  benchTreeSetup("tve kdtree-setup pointxyz 64K", target_64k);
  benchTreeSetup("tve kdtree-setup pointxyz 256K", target_256k);
  benchSearchOnly("tve search-only negative-control pointxyz 64K", source_64k);
  benchSearchOnly("tve search-only negative-control pointxyz 256K", source_256k);
  benchFullValidation("tve full-validation fresh-tree pointxyz 64K", source_64k);
  benchFullValidation("tve full-validation fresh-tree pointxyz 256K", source_256k);
  benchFullValidationPrebuiltTree(
      "tve full-validation prebuilt-tree pointxyz 64K", source_64k);
  benchFullValidationPrebuiltTree(
      "tve full-validation prebuilt-tree pointxyz 256K", source_256k);

  printBanner('=');
  return 0;
}
