#include <pcl/filters/fast_bilateral.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>

#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

#if defined(__RVV10__)
#include <riscv_vector.h>
#endif

namespace {

struct CaseConfig {
  std::string name;
  std::uint32_t width;
  std::uint32_t height;
  float sigma_s;
  float sigma_r;
  bool inject_non_finite;
};

struct LatticeCell {
  float sum;
  float count;
};

pcl::PointCloud<pcl::PointXYZ>::Ptr
makeCloud(const CaseConfig& cfg)
{
  auto cloud = pcl::make_shared<pcl::PointCloud<pcl::PointXYZ>>();
  cloud->width = cfg.width;
  cloud->height = cfg.height;
  cloud->is_dense = !cfg.inject_non_finite;
  cloud->points.resize(static_cast<std::size_t>(cfg.width) * cfg.height);

  for (std::uint32_t y = 0; y < cfg.height; ++y) {
    for (std::uint32_t x = 0; x < cfg.width; ++x) {
      const std::size_t idx = static_cast<std::size_t>(y) * cfg.width + x;
      const float fx = static_cast<float>(x);
      const float fy = static_cast<float>(y);
      const float ripple = 0.025f * std::sin(fx * 0.07f) + 0.035f * std::cos(fy * 0.05f);
      (*cloud)[idx].x = fx * 0.01f;
      (*cloud)[idx].y = fy * 0.01f;
      (*cloud)[idx].z = 0.75f + 0.0015f * fx + 0.0025f * fy + ripple;
    }
  }

  if (cfg.inject_non_finite && cloud->size() > 1024) {
    for (std::size_t i = 37; i < cloud->size(); i += 997)
      (*cloud)[i].z = std::numeric_limits<float>::quiet_NaN();
    for (std::size_t i = 211; i < cloud->size(); i += 1601)
      (*cloud)[i].z = std::numeric_limits<float>::infinity();
    for (std::size_t i = 503; i < cloud->size(); i += 2039)
      (*cloud)[i].z = -std::numeric_limits<float>::infinity();
  }

  return cloud;
}

std::uint64_t
checksumCloud(const pcl::PointCloud<pcl::PointXYZ>& cloud)
{
  std::uint64_t hash = 1469598103934665603ull;
  for (const auto& point : cloud) {
    const int bucket = std::isfinite(point.z) ? static_cast<int>(std::lround(point.z * 100000.0f)) : 0;
    hash ^= static_cast<std::uint64_t>(bucket + 0x9e3779b9);
    hash *= 1099511628211ull;
  }
  return hash;
}

std::vector<LatticeCell>
makeLattice(std::size_t x_dim, std::size_t y_dim, std::size_t z_dim)
{
  std::vector<LatticeCell> data(x_dim * y_dim * z_dim);
  for (std::size_t x = 0; x < x_dim; ++x) {
    for (std::size_t y = 0; y < y_dim; ++y) {
      for (std::size_t z = 0; z < z_dim; ++z) {
        const std::size_t idx = (x * y_dim + y) * z_dim + z;
        data[idx].sum = 0.5f + 0.01f * static_cast<float>((x * 13 + y * 7 + z * 3) % 101);
        data[idx].count = 1.0f + 0.02f * static_cast<float>((x * 5 + y * 11 + z * 17) % 37);
      }
    }
  }
  return data;
}

void
blurLatticeScalar(std::vector<LatticeCell>& data, std::vector<LatticeCell>& buffer,
                  std::size_t x_dim, std::size_t y_dim, std::size_t z_dim)
{
  const std::ptrdiff_t offsets[3] = {
      static_cast<std::ptrdiff_t>(y_dim * z_dim),
      static_cast<std::ptrdiff_t>(z_dim),
      1};

  for (std::size_t dim = 0; dim < 3; ++dim) {
    const std::ptrdiff_t off = offsets[dim];
    for (std::size_t iter = 0; iter < 2; ++iter) {
      std::swap(buffer, data);
      for (std::size_t x = 1; x < x_dim - 1; ++x) {
        for (std::size_t y = 1; y < y_dim - 1; ++y) {
          const std::size_t base = (x * y_dim + y) * z_dim;
          for (std::size_t z = 1; z < z_dim - 1; ++z) {
            const std::size_t idx = base + z;
            data[idx].sum = (buffer[idx - off].sum + buffer[idx + off].sum + 2.0f * buffer[idx].sum) * 0.25f;
            data[idx].count = (buffer[idx - off].count + buffer[idx + off].count + 2.0f * buffer[idx].count) * 0.25f;
          }
        }
      }
    }
  }
}

#if defined(__RVV10__)
void
blurLatticeRVV(std::vector<LatticeCell>& data, std::vector<LatticeCell>& buffer,
               std::size_t x_dim, std::size_t y_dim, std::size_t z_dim)
{
  const std::ptrdiff_t offsets[3] = {
      static_cast<std::ptrdiff_t>(y_dim * z_dim),
      static_cast<std::ptrdiff_t>(z_dim),
      1};
  const auto cell_stride = static_cast<std::ptrdiff_t>(sizeof(LatticeCell));

  for (std::size_t dim = 0; dim < 3; ++dim) {
    const std::ptrdiff_t off_bytes = offsets[dim] * cell_stride;
    for (std::size_t iter = 0; iter < 2; ++iter) {
      std::swap(buffer, data);
      for (std::size_t x = 1; x < x_dim - 1; ++x) {
        for (std::size_t y = 1; y < y_dim - 1; ++y) {
          std::size_t z = 1;
          while (z < z_dim - 1) {
            const std::size_t vl = __riscv_vsetvl_e32m2(z_dim - 1 - z);
            const std::size_t idx = (x * y_dim + y) * z_dim + z;
            const auto* b_base = reinterpret_cast<const std::uint8_t*>(buffer.data() + idx);
            auto* d_base = reinterpret_cast<std::uint8_t*>(data.data() + idx);

            // Bench-only probe for the FastBilateral data/buffer blur loop.
            // LatticeCell mirrors Eigen::Vector2f storage, so sum/count are
            // two strided float channels across a VL chunk in z.
            const auto* prev_sum = reinterpret_cast<const float*>(b_base - off_bytes);
            const auto* curr_sum = reinterpret_cast<const float*>(b_base);
            const auto* next_sum = reinterpret_cast<const float*>(b_base + off_bytes);
            auto* out_sum = reinterpret_cast<float*>(d_base);
            const vfloat32m2_t prev_s = __riscv_vlse32_v_f32m2(prev_sum, cell_stride, vl);
            const vfloat32m2_t curr_s = __riscv_vlse32_v_f32m2(curr_sum, cell_stride, vl);
            const vfloat32m2_t next_s = __riscv_vlse32_v_f32m2(next_sum, cell_stride, vl);
            vfloat32m2_t out_s = __riscv_vfadd_vv_f32m2(prev_s, next_s, vl);
            out_s = __riscv_vfmacc_vf_f32m2(out_s, 2.0f, curr_s, vl);
            out_s = __riscv_vfmul_vf_f32m2(out_s, 0.25f, vl);
            __riscv_vsse32_v_f32m2(out_sum, cell_stride, out_s, vl);

            const auto* prev_count = reinterpret_cast<const float*>(b_base - off_bytes + sizeof(float));
            const auto* curr_count = reinterpret_cast<const float*>(b_base + sizeof(float));
            const auto* next_count = reinterpret_cast<const float*>(b_base + off_bytes + sizeof(float));
            auto* out_count = reinterpret_cast<float*>(d_base + sizeof(float));
            const vfloat32m2_t prev_c = __riscv_vlse32_v_f32m2(prev_count, cell_stride, vl);
            const vfloat32m2_t curr_c = __riscv_vlse32_v_f32m2(curr_count, cell_stride, vl);
            const vfloat32m2_t next_c = __riscv_vlse32_v_f32m2(next_count, cell_stride, vl);
            vfloat32m2_t out_c = __riscv_vfadd_vv_f32m2(prev_c, next_c, vl);
            out_c = __riscv_vfmacc_vf_f32m2(out_c, 2.0f, curr_c, vl);
            out_c = __riscv_vfmul_vf_f32m2(out_c, 0.25f, vl);
            __riscv_vsse32_v_f32m2(out_count, cell_stride, out_c, vl);

            z += vl;
          }
        }
      }
    }
  }
}
#endif

std::uint64_t
checksumLattice(const std::vector<LatticeCell>& data)
{
  std::uint64_t hash = 1469598103934665603ull;
  for (const auto& cell : data) {
    const int sum_bucket = static_cast<int>(std::lround(cell.sum * 100000.0f));
    const int count_bucket = static_cast<int>(std::lround(cell.count * 100000.0f));
    hash ^= static_cast<std::uint64_t>(sum_bucket + 0x9e3779b9);
    hash *= 1099511628211ull;
    hash ^= static_cast<std::uint64_t>(count_bucket + 0x85ebca6b);
    hash *= 1099511628211ull;
  }
  return hash;
}

double
runCase(const CaseConfig& cfg, int iterations, std::uint64_t& checksum)
{
  const auto cloud = makeCloud(cfg);
  pcl::PointCloud<pcl::PointXYZ> output;
  checksum = 0;

  auto start = std::chrono::steady_clock::now();
  for (int i = 0; i < iterations; ++i) {
    pcl::FastBilateralFilter<pcl::PointXYZ> filter;
    filter.setInputCloud(cloud);
    filter.setSigmaS(cfg.sigma_s);
    filter.setSigmaR(cfg.sigma_r);
    filter.filter(output);
    checksum ^= checksumCloud(output) + static_cast<std::uint64_t>(i);
  }
  auto stop = std::chrono::steady_clock::now();

  return std::chrono::duration<double, std::milli>(stop - start).count();
}

double
runBlurCase(std::size_t x_dim, std::size_t y_dim, std::size_t z_dim, int iterations, std::uint64_t& checksum)
{
  checksum = 0;
  auto start = std::chrono::steady_clock::now();
  for (int i = 0; i < iterations; ++i) {
    auto data = makeLattice(x_dim, y_dim, z_dim);
    auto buffer = makeLattice(x_dim, y_dim, z_dim);
#if defined(__RVV10__)
    blurLatticeRVV(data, buffer, x_dim, y_dim, z_dim);
#else
    blurLatticeScalar(data, buffer, x_dim, y_dim, z_dim);
#endif
    checksum ^= checksumLattice(data) + static_cast<std::uint64_t>(i);
  }
  auto stop = std::chrono::steady_clock::now();
  return std::chrono::duration<double, std::milli>(stop - start).count();
}

} // namespace

int
main()
{
  const int iterations = 8;
  const std::vector<CaseConfig> cases = {
      {"fast_bilateral organized 64x48 finite", 64, 48, 4.0f, 0.05f, false},
      {"fast_bilateral organized 160x120 finite", 160, 120, 5.0f, 0.05f, false},
      {"fast_bilateral organized 160x120 nonfinite", 160, 120, 5.0f, 0.05f, true},
      {"fast_bilateral organized 320x240 finite", 320, 240, 6.0f, 0.05f, false},
      {"fast_bilateral organized 320x240 nonfinite", 320, 240, 6.0f, 0.05f, true},
      {"fast_bilateral small fallback 7x5", 7, 5, 3.0f, 0.05f, true},
  };

  std::cout << "==== PCL filters/fast_bilateral RVV bench ====\n";
  std::cout << "Dataset: synthetic organized PointXYZ depth images; finite and non-finite z cases\n";
  std::cout << "Iterations: " << iterations << "\n";
#if defined(__RVV10__)
  std::cout << "Build: RVV\n";
#else
  std::cout << "Build: Std\n";
#endif

  for (const auto& cfg : cases) {
    std::uint64_t checksum = 0;
    const double total_ms = runCase(cfg, iterations, checksum);
    const double avg_ms = total_ms / static_cast<double>(iterations);
    std::cout << cfg.name << " : " << std::fixed << std::setprecision(6) << avg_ms << " ms/iter\n";
    std::cout << "  Total Time: " << std::fixed << std::setprecision(6) << total_ms
              << " ms, checksum: " << checksum
              << ", size: " << cfg.width << "x" << cfg.height
              << ", sigma_s: " << cfg.sigma_s
              << ", sigma_r: " << cfg.sigma_r
              << ", nonfinite: " << (cfg.inject_non_finite ? "yes" : "no") << "\n";
  }

  std::uint64_t blur_checksum = 0;
  const double blur_total_ms = runBlurCase(96, 72, 64, iterations, blur_checksum);
  const double blur_avg_ms = blur_total_ms / static_cast<double>(iterations);
  std::cout << "fast_bilateral blur lattice 96x72x64 : "
            << std::fixed << std::setprecision(6) << blur_avg_ms << " ms/iter\n";
  std::cout << "  Total Time: " << std::fixed << std::setprecision(6) << blur_total_ms
            << " ms, checksum: " << blur_checksum
            << ", lattice: 96x72x64"
#if defined(__RVV10__)
            << ", kernel: RVV experimental blur"
#else
            << ", kernel: Std experimental blur"
#endif
            << "\n";

  return 0;
}
