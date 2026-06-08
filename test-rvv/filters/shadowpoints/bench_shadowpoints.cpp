#include <pcl/filters/shadowpoints.h>
#include <pcl/common/rvv_point_load.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>

#include <chrono>
#include <cstdint>
#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <type_traits>

#if defined(__RVV10__)
#include <limits>
#include <riscv_vector.h>
#endif

namespace {

constexpr int kBenchmarkIterations = 30;
constexpr std::size_t kBenchmarkBannerWidth = 96;

void
printBanner(char ch)
{
  std::cout << std::string(kBenchmarkBannerWidth, ch) << '\n';
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

  void run(const std::function<void()>& func, int iterations = kBenchmarkIterations, int warmup = 5) const
  {
    for (int i = 0; i < warmup; ++i)
      func();
    const auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i)
      func();
    const auto end = std::chrono::high_resolution_clock::now();
    const double total_ms = std::chrono::duration<double, std::milli>(end - start).count();
    std::cout << std::left << std::setw(54) << name_ << ": " << std::fixed
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
  auto cloud = std::make_shared<pcl::PointCloud<pcl::PointXYZ>>();
  cloud->width = static_cast<std::uint32_t>(n);
  cloud->height = 1;
  cloud->is_dense = false;
  cloud->points.resize(n);

  for (std::size_t i = 0; i < n; ++i) {
    (*cloud)[i].x = static_cast<float>(static_cast<int>(i % 4099) - 2048) / 180.0f;
    (*cloud)[i].y = static_cast<float>(static_cast<int>((i * 7) % 4093) - 2046) / 250.0f;
    (*cloud)[i].z = static_cast<float>(static_cast<int>((i * 13) % 4091) - 2045) / 260.0f;
  }
  return cloud;
}

pcl::PointCloud<pcl::PointXYZI>::Ptr
makeCloudXYZI(std::size_t n)
{
  auto cloud = std::make_shared<pcl::PointCloud<pcl::PointXYZI>>();
  cloud->width = static_cast<std::uint32_t>(n);
  cloud->height = 1;
  cloud->is_dense = true;
  cloud->points.resize(n);

  for (std::size_t i = 0; i < n; ++i) {
    (*cloud)[i].x = static_cast<float>(static_cast<int>(i % 257) - 128) / 60.0f;
    (*cloud)[i].y = static_cast<float>(static_cast<int>((i * 5) % 263) - 131) / 90.0f;
    (*cloud)[i].z = static_cast<float>(static_cast<int>((i * 11) % 269) - 134) / 95.0f;
    (*cloud)[i].intensity = static_cast<float>(i % 17);
  }
  return cloud;
}

pcl::PointCloud<pcl::PointNormal>::Ptr
makePointNormals(std::size_t n)
{
  auto normals = std::make_shared<pcl::PointCloud<pcl::PointNormal>>();
  normals->width = static_cast<std::uint32_t>(n);
  normals->height = 1;
  normals->is_dense = true;
  normals->points.resize(n);

  for (std::size_t i = 0; i < n; ++i) {
    (*normals)[i].normal_x = (i % 3 == 0) ? 0.00f : 0.16f;
    (*normals)[i].normal_y = (i % 5 == 0) ? 0.00f : -0.09f;
    (*normals)[i].normal_z = (i % 2 == 0) ? 1.00f : 0.36f;
  }
  return normals;
}

pcl::PointCloud<pcl::Normal>::Ptr
makePlainNormals(std::size_t n)
{
  auto normals = std::make_shared<pcl::PointCloud<pcl::Normal>>();
  normals->width = static_cast<std::uint32_t>(n);
  normals->height = 1;
  normals->is_dense = true;
  normals->points.resize(n);

  for (std::size_t i = 0; i < n; ++i) {
    (*normals)[i].normal_x = 0.03f;
    (*normals)[i].normal_y = -0.05f;
    (*normals)[i].normal_z = (i % 2 == 0) ? 1.00f : 0.28f;
  }
  return normals;
}

pcl::IndicesPtr
makeSubset(std::size_t n)
{
  auto indices = std::make_shared<pcl::Indices>();
  indices->reserve(n / 2);
  for (std::size_t i = 1; i < n; i += 2)
    indices->push_back(static_cast<int>(i));
  return indices;
}

std::uint64_t
checksumIndices(const pcl::Indices& indices)
{
  std::uint64_t sum = 1469598103934665603ull;
  for (const int v : indices)
    sum = (sum ^ static_cast<std::uint32_t>(v)) * 1099511628211ull;
  return sum;
}

#if defined(__RVV10__)

bool
shadowPointsBenchOnlyRVV(const pcl::PointCloud<pcl::PointXYZ>& cloud,
                         const pcl::PointCloud<pcl::PointNormal>& normals,
                         pcl::Indices& indices,
                         pcl::Indices& removed,
                         bool extract_removed,
                         bool negative,
                         float threshold)
{
  const std::size_t n = cloud.size();
  if (normals.size() < n || n > static_cast<std::size_t>(std::numeric_limits<int>::max()))
    return false;

  indices.resize(n);
  if (extract_removed)
    removed.resize(n);
  else
    removed.clear();

  const auto* point_base = reinterpret_cast<const std::uint8_t*>(cloud.data());
  const auto* normal_base = reinterpret_cast<const std::uint8_t*>(normals.data());
  int* out = indices.data();
  int* removed_out = extract_removed ? removed.data() : nullptr;
  std::size_t kept = 0;
  std::size_t dropped = 0;
  std::size_t i = 0;

  while (i < n) {
    const std::size_t vl = __riscv_vsetvl_e32m2(n - i);
    const auto* point_chunk = point_base + i * sizeof(pcl::PointXYZ);
    const auto* normal_chunk = normal_base + i * sizeof(pcl::PointNormal);

    vfloat32m2_t px;
    vfloat32m2_t py;
    vfloat32m2_t pz;
    pcl::rvv_load::strided_load3_f32m2<sizeof(pcl::PointXYZ),
                                       offsetof(pcl::PointXYZ, x),
                                       offsetof(pcl::PointXYZ, y),
                                       offsetof(pcl::PointXYZ, z)>(point_chunk, vl, px, py, pz);

    vfloat32m2_t nx;
    vfloat32m2_t ny;
    vfloat32m2_t nz;
    pcl::rvv_load::strided_load3_fields_f32m2<sizeof(pcl::PointNormal),
                                              offsetof(pcl::PointNormal, normal_x),
                                              offsetof(pcl::PointNormal, normal_y),
                                              offsetof(pcl::PointNormal, normal_z)>(
        normal_chunk, vl, nx, ny, nz);

    vfloat32m2_t dot = __riscv_vfmul_vv_f32m2(nx, px, vl);
    dot = __riscv_vfmacc_vv_f32m2(dot, ny, py, vl);
    dot = __riscv_vfmacc_vv_f32m2(dot, nz, pz, vl);
    const vfloat32m2_t abs_dot = __riscv_vfabs_v_f32m2(dot, vl);
    const vbool16_t inlier = __riscv_vmfge_vf_f32m2_b16(abs_dot, threshold, vl);
    const vbool16_t keep = negative ? __riscv_vmnot_m_b16(inlier, vl) : inlier;
    const vbool16_t removed_mask = negative ? inlier : __riscv_vmnot_m_b16(inlier, vl);

    const vuint32m2_t local = __riscv_vid_v_u32m2(vl);
    const vuint32m2_t source = __riscv_vadd_vx_u32m2(local, static_cast<std::uint32_t>(i), vl);
    const vint32m2_t source_i32 = __riscv_vreinterpret_v_u32m2_i32m2(source);

    const vint32m2_t kept_i32 = __riscv_vcompress_vm_i32m2(source_i32, keep, vl);
    const std::size_t keep_count = __riscv_vcpop_m_b16(keep, vl);
    __riscv_vse32_v_i32m2(out + kept, kept_i32, keep_count);
    kept += keep_count;

    if (extract_removed) {
      const vint32m2_t removed_i32 = __riscv_vcompress_vm_i32m2(source_i32, removed_mask, vl);
      const std::size_t removed_count = __riscv_vcpop_m_b16(removed_mask, vl);
      __riscv_vse32_v_i32m2(removed_out + dropped, removed_i32, removed_count);
      dropped += removed_count;
    }

    i += vl;
  }

  indices.resize(kept);
  removed.resize(extract_removed ? dropped : 0);
  return true;
}

#endif

template <typename PointT, typename NormalT>
void
benchIndices(const std::string& name,
             const typename pcl::PointCloud<PointT>::Ptr& cloud,
             const typename pcl::PointCloud<NormalT>::Ptr& normals,
             bool negative = false,
             bool extract_removed = false,
             const pcl::IndicesPtr& subset = {})
{
  Benchmarker bench(name);
#if defined(__RVV10__) && defined(PCL_SHADOWPOINTS_RVV_BENCH_ONLY)
  if constexpr (std::is_same_v<PointT, pcl::PointXYZ> && std::is_same_v<NormalT, pcl::PointNormal>) {
    if (!subset) {
      auto removed = std::make_shared<pcl::Indices>();
      pcl::Indices indices;
      bench.run([&]() {
        shadowPointsBenchOnlyRVV(*cloud, *normals, indices, *removed, extract_removed, negative, 0.1f);
        std::uint64_t checksum = checksumIndices(indices);
        if (extract_removed)
          checksum ^= checksumIndices(*removed);
        bench.setChecksum(checksum);
        doNotOptimize(indices);
      });
      return;
    }
  }
#endif

  pcl::ShadowPoints<PointT, NormalT> filter(extract_removed);
  filter.setInputCloud(cloud);
  filter.setNormals(normals);
  filter.setThreshold(0.1f);
  filter.setNegative(negative);
  if (subset)
    filter.setIndices(subset);

  pcl::Indices indices;
  bench.run([&]() {
    filter.filter(indices);
    std::uint64_t checksum = checksumIndices(indices);
    if (extract_removed)
      checksum ^= checksumIndices(*filter.getRemovedIndices());
    bench.setChecksum(checksum);
    doNotOptimize(indices);
  });
}

void
benchCloudOutput(const std::string& name,
                 const pcl::PointCloud<pcl::PointXYZ>::Ptr& cloud,
                 const pcl::PointCloud<pcl::PointNormal>::Ptr& normals)
{
  Benchmarker bench(name);
  pcl::ShadowPoints<pcl::PointXYZ, pcl::PointNormal> filter(true);
  filter.setInputCloud(cloud);
  filter.setNormals(normals);
  filter.setThreshold(0.1f);
  filter.setKeepOrganized(true);

  pcl::PointCloud<pcl::PointXYZ> output;
  bench.run([&]() {
    filter.filter(output);
    bench.setChecksum(static_cast<std::uint64_t>(output.size()) ^
                      (static_cast<std::uint64_t>(filter.getRemovedIndices()->size()) << 32));
    doNotOptimize(output);
  });
}

} // namespace

int
main()
{
  printBanner('=');
  std::cout << " PCL filters/shadowpoints Benchmark (ShadowPoints<PointXYZ,PointNormal> indices RVV path)\n";
  printBanner('=');
#if defined(__RVV10__)
  std::cout << "Build: RVV (__RVV10__ enabled";
#if defined(PCL_SHADOWPOINTS_RVV_BENCH_ONLY)
  std::cout << ", bench-only helper enabled";
#endif
  std::cout << ")\n";
#else
  std::cout << "Build: Std (__RVV10__ disabled)\n";
#endif
  std::cout << "Dataset: synthetic PointXYZ + PointNormal clouds; full-cloud dot/abs threshold bench-only RVV cases and subset/type/cloud-output fallback cases\n";
  std::cout << "Iterations: " << kBenchmarkIterations << '\n';

  const auto cloud64k = makeCloud(64 * 1024);
  const auto normals64k = makePointNormals(cloud64k->size());
  const auto cloud1m = makeCloud(1024 * 1024);
  const auto normals1m = makePointNormals(cloud1m->size());
  const auto cloudXYZI = makeCloudXYZI(1024 * 1024);
  const auto plainNormals = makePlainNormals(cloud1m->size());

  benchIndices<pcl::PointXYZ, pcl::PointNormal>("shadowpoints pointxyz full-cloud 64K", cloud64k, normals64k);
  benchIndices<pcl::PointXYZ, pcl::PointNormal>("shadowpoints pointxyz full-cloud 1M", cloud1m, normals1m);
  benchIndices<pcl::PointXYZ, pcl::PointNormal>("shadowpoints negative removed 1M", cloud1m, normals1m, true, true);
  benchIndices<pcl::PointXYZ, pcl::PointNormal>("shadowpoints subset fallback 1M", cloud1m, normals1m, false, false, makeSubset(cloud1m->size()));
  benchIndices<pcl::PointXYZ, pcl::Normal>("shadowpoints normal type fallback 1M", cloud1m, plainNormals);
  benchIndices<pcl::PointXYZI, pcl::PointNormal>("shadowpoints pointxyzi fallback 1M", cloudXYZI, normals1m);
  benchCloudOutput("shadowpoints cloud keep_organized fallback 64K", cloud64k, normals64k);

  return 0;
}
