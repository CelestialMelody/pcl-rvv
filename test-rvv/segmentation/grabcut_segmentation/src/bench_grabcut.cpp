#include <grabcut_diagnostic.h>
#include <pcl/console/print.h>
#include <pcl/segmentation/grabcut_segmentation.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

struct BenchConfig {
  std::uint32_t width = 640;
  std::uint32_t height = 480;
  int iterations = 8;
  int warmup_iterations = 2;
  float lambda = 50.0f;
  std::string bench_case = "all";
};

BenchConfig
parseArgs(int argc, char** argv)
{
  BenchConfig config;
  for (int i = 1; i + 1 < argc; i += 2) {
    const std::string key = argv[i];
    if (key == "--width") {
      config.width = static_cast<std::uint32_t>(std::atoi(argv[i + 1]));
    }
    else if (key == "--height") {
      config.height = static_cast<std::uint32_t>(std::atoi(argv[i + 1]));
    }
    else if (key == "--iterations") {
      config.iterations = std::atoi(argv[i + 1]);
    }
    else if (key == "--warmup") {
      config.warmup_iterations = std::atoi(argv[i + 1]);
    }
    else if (key == "--lambda") {
      config.lambda = std::atof(argv[i + 1]);
    }
    else if (key == "--case") {
      config.bench_case = argv[i + 1];
    }
  }
  if (config.width == 0 || config.height == 0 || config.iterations <= 0 || config.warmup_iterations < 0) {
    throw std::invalid_argument("invalid bench arguments");
  }
  if (config.bench_case != "all" && config.bench_case != "organized_nlinks" &&
      config.bench_case != "gmm_probability" && config.bench_case != "terminal_weights" &&
      config.bench_case != "initgraph_no_solve" &&
      config.bench_case != "production_initgraph_terminal" &&
      config.bench_case != "public_extract" &&
      config.bench_case != "public_extract_profile" &&
      config.bench_case != "learn_gmm_assignment" &&
      config.bench_case != "learn_gmms_full" &&
      config.bench_case != "production_learn_gmms") {
    throw std::invalid_argument(
        "invalid --case; expected all, organized_nlinks, gmm_probability, terminal_weights, initgraph_no_solve, production_initgraph_terminal, public_extract, public_extract_profile, learn_gmm_assignment, learn_gmms_full, or production_learn_gmms");
  }
  return config;
}

std::vector<grabcut_diag::Color>
makeOrganizedImage(std::uint32_t width, std::uint32_t height)
{
  std::vector<grabcut_diag::Color> image;
  image.resize(static_cast<std::size_t>(width) * height);
  for (std::uint32_t y = 0; y < height; ++y) {
    for (std::uint32_t x = 0; x < width; ++x) {
      const std::size_t index = static_cast<std::size_t>(y) * width + x;
      image[index].r = static_cast<float>((x * 17 + y * 3) % 256) / 255.0f;
      image[index].g = static_cast<float>((x * 5 + y * 29) % 256) / 255.0f;
      image[index].b = static_cast<float>((x * 11 + y * 13) % 256) / 255.0f;
    }
  }
  return image;
}

pcl::PointCloud<pcl::PointXYZRGB>::Ptr
makeOrganizedCloud(const std::vector<grabcut_diag::Color>& image,
                   std::uint32_t width,
                   std::uint32_t height)
{
  auto cloud = pcl::make_shared<pcl::PointCloud<pcl::PointXYZRGB>>();
  cloud->width = width;
  cloud->height = height;
  cloud->is_dense = true;
  cloud->points.resize(image.size());
  for (std::uint32_t y = 0; y < height; ++y) {
    for (std::uint32_t x = 0; x < width; ++x) {
      const std::size_t index = static_cast<std::size_t>(y) * width + x;
      auto& point = cloud->points[index];
      point.x = static_cast<float>(x) / static_cast<float>(std::max<std::uint32_t>(width, 1));
      point.y = static_cast<float>(y) / static_cast<float>(std::max<std::uint32_t>(height, 1));
      point.z = static_cast<float>((x * 7 + y * 11) % 97) / 97.0f;
      point.r = static_cast<std::uint8_t>(std::round(image[index].r * 255.0f));
      point.g = static_cast<std::uint8_t>(std::round(image[index].g * 255.0f));
      point.b = static_cast<std::uint8_t>(std::round(image[index].b * 255.0f));
    }
  }
  return cloud;
}

pcl::PointIndices::Ptr
makeCentralUnknownIndices(std::uint32_t width, std::uint32_t height)
{
  auto indices = pcl::make_shared<pcl::PointIndices>();
  const std::uint32_t x_begin = width / 4;
  const std::uint32_t x_end = width - width / 4;
  const std::uint32_t y_begin = height / 4;
  const std::uint32_t y_end = height - height / 4;
  indices->indices.reserve(static_cast<std::size_t>(x_end - x_begin) * (y_end - y_begin));
  for (std::uint32_t y = y_begin; y < y_end; ++y) {
    for (std::uint32_t x = x_begin; x < x_end; ++x) {
      indices->indices.push_back(static_cast<int>(static_cast<std::size_t>(y) * width + x));
    }
  }
  return indices;
}

std::vector<unsigned char>
makeLearnForegroundMask(std::uint32_t width, std::uint32_t height)
{
  std::vector<unsigned char> mask(static_cast<std::size_t>(width) * height, 0);
  const std::uint32_t x_begin = width / 4;
  const std::uint32_t x_end = width - width / 4;
  const std::uint32_t y_begin = height / 4;
  const std::uint32_t y_end = height - height / 4;
  for (std::uint32_t y = y_begin; y < y_end; ++y) {
    for (std::uint32_t x = x_begin; x < x_end; ++x) {
      mask[static_cast<std::size_t>(y) * width + x] = 1;
    }
  }
  return mask;
}

std::uint64_t
mixHash(std::uint64_t hash, std::uint64_t value)
{
  hash ^= value;
  hash *= 1099511628211ull;
  return hash;
}

std::uint64_t
quantizeFloat(float value)
{
  return static_cast<std::uint64_t>(static_cast<std::int64_t>(value * 1000000.0f));
}

std::uint64_t
checksumClusters(const std::vector<pcl::PointIndices>& clusters)
{
  std::uint64_t hash = 1469598103934665603ull;
  const auto mix = [&hash](std::uint64_t value) { hash = mixHash(hash, value); };
  mix(static_cast<std::uint64_t>(clusters.size()));
  for (const auto& cluster : clusters) {
    mix(static_cast<std::uint64_t>(cluster.indices.size()));
    for (const auto index : cluster.indices) {
      mix(static_cast<std::uint64_t>(index + 2));
    }
  }
  return hash;
}

std::uint64_t
checksumOrganizedNLinks(const grabcut_diag::OrganizedNLinksResult& result)
{
  std::uint64_t hash = 1469598103934665603ull;
  const auto mix = [&hash](std::uint64_t value) { hash = mixHash(hash, value); };
  mix(static_cast<std::uint64_t>(result.links.size()));
  mix(static_cast<std::uint64_t>(result.beta * 1000000.0f));
  for (const auto& link : result.links) {
    mix(static_cast<std::uint64_t>(link.valid_count));
    for (std::size_t i = 0; i < link.weights.size(); ++i) {
      mix(static_cast<std::uint64_t>(link.indices[i] + 2));
      mix(quantizeFloat(link.weights[i]));
    }
  }
  return hash;
}

grabcut_diag::Gaussian
makeGaussian()
{
  grabcut_diag::Gaussian gaussian;
  gaussian.mu = {0.35f, 0.45f, 0.25f};
  gaussian.determinant = 0.21875f;
  gaussian.inverse = {
      2.10f, 0.12f, 0.05f,
      0.12f, 1.80f, 0.08f,
      0.05f, 0.08f, 2.40f,
  };
  return gaussian;
}

grabcut_diag::MixtureGMM
makeBackgroundGMM()
{
  grabcut_diag::MixtureGMM gmm;
  gmm.components = {
      {{0.15f, 0.25f, 0.20f}, {2.00f, 0.10f, 0.04f, 0.10f, 1.70f, 0.06f, 0.04f, 0.06f, 2.30f}, 0.310f, 0.38f},
      {{0.35f, 0.45f, 0.30f}, {1.80f, 0.08f, 0.03f, 0.08f, 1.55f, 0.05f, 0.03f, 0.05f, 2.05f}, 0.420f, 0.27f},
      {{0.55f, 0.20f, 0.55f}, {2.20f, 0.07f, 0.02f, 0.07f, 1.90f, 0.04f, 0.02f, 0.04f, 1.85f}, 0.360f, 0.18f},
      {{0.70f, 0.65f, 0.40f}, {1.65f, 0.06f, 0.01f, 0.06f, 1.80f, 0.03f, 0.01f, 0.03f, 2.10f}, 0.390f, 0.11f},
      {{0.25f, 0.75f, 0.70f}, {2.35f, 0.05f, 0.02f, 0.05f, 1.60f, 0.02f, 0.02f, 0.02f, 1.95f}, 0.280f, 0.06f},
  };
  return gmm;
}

grabcut_diag::MixtureGMM
makeForegroundGMM()
{
  grabcut_diag::MixtureGMM gmm;
  gmm.components = {
      {{0.75f, 0.25f, 0.18f}, {1.75f, 0.09f, 0.03f, 0.09f, 2.10f, 0.05f, 0.03f, 0.05f, 1.80f}, 0.330f, 0.34f},
      {{0.60f, 0.50f, 0.32f}, {2.05f, 0.11f, 0.04f, 0.11f, 1.85f, 0.07f, 0.04f, 0.07f, 2.20f}, 0.450f, 0.26f},
      {{0.40f, 0.70f, 0.52f}, {1.95f, 0.04f, 0.02f, 0.04f, 2.25f, 0.06f, 0.02f, 0.06f, 1.90f}, 0.370f, 0.19f},
      {{0.20f, 0.55f, 0.78f}, {2.40f, 0.03f, 0.01f, 0.03f, 1.70f, 0.04f, 0.01f, 0.04f, 2.05f}, 0.290f, 0.13f},
      {{0.85f, 0.80f, 0.68f}, {1.60f, 0.05f, 0.02f, 0.05f, 1.95f, 0.03f, 0.02f, 0.03f, 2.30f}, 0.410f, 0.08f},
  };
  return gmm;
}

void
copyDiagnosticGMMToProduction(const grabcut_diag::MixtureGMM& source,
                              pcl::segmentation::grabcut::GMM& dest)
{
  dest.resize(source.components.size());
  for (std::size_t i = 0; i < source.components.size(); ++i) {
    auto& out = dest[i];
    const auto& in = source.components[i];
    out.mu.r = in.mu.r;
    out.mu.g = in.mu.g;
    out.mu.b = in.mu.b;
    out.determinant = in.determinant;
    out.pi = in.pi;
    for (std::size_t row = 0; row < 3; ++row) {
      for (std::size_t col = 0; col < 3; ++col) {
        out.inverse(row, col) = in.inverse(row, col);
      }
    }
  }
}

pcl::segmentation::grabcut::Image
makeProductionImage(const std::vector<grabcut_diag::Color>& colors)
{
  pcl::segmentation::grabcut::Image image;
  image.resize(colors.size());
  image.width = static_cast<std::uint32_t>(colors.size());
  image.height = 1;
  for (std::size_t i = 0; i < colors.size(); ++i) {
    image[i].r = colors[i].r;
    image[i].g = colors[i].g;
    image[i].b = colors[i].b;
  }
  return image;
}

std::vector<pcl::segmentation::grabcut::SegmentationValue>
makeHardSegmentation(const std::vector<unsigned char>& foreground_mask)
{
  std::vector<pcl::segmentation::grabcut::SegmentationValue> hard_segmentation;
  hard_segmentation.reserve(foreground_mask.size());
  for (const auto value : foreground_mask) {
    hard_segmentation.push_back(value ? pcl::segmentation::grabcut::SegmentationForeground
                                      : pcl::segmentation::grabcut::SegmentationBackground);
  }
  return hard_segmentation;
}

void
mixGaussian(std::uint64_t& hash, const grabcut_diag::Gaussian& gaussian)
{
  const auto mix = [&hash](std::uint64_t value) { hash = mixHash(hash, value); };
  mix(quantizeFloat(gaussian.mu.r));
  mix(quantizeFloat(gaussian.mu.g));
  mix(quantizeFloat(gaussian.mu.b));
  mix(quantizeFloat(gaussian.determinant));
  mix(quantizeFloat(gaussian.pi));
  for (std::size_t i = 0; i < 9; ++i) {
    mix(quantizeFloat(gaussian.inverse.values[i]));
  }
}

void
mixGaussian(std::uint64_t& hash, const pcl::segmentation::grabcut::Gaussian& gaussian)
{
  const auto mix = [&hash](std::uint64_t value) { hash = mixHash(hash, value); };
  mix(quantizeFloat(gaussian.mu.r));
  mix(quantizeFloat(gaussian.mu.g));
  mix(quantizeFloat(gaussian.mu.b));
  mix(quantizeFloat(gaussian.determinant));
  mix(quantizeFloat(gaussian.pi));
  for (std::size_t i = 0; i < 9; ++i) {
    mix(quantizeFloat(gaussian.inverse(i / 3, i % 3)));
  }
}

std::uint64_t
checksumGMMInputFingerprint(const std::vector<grabcut_diag::Color>& image,
                            const grabcut_diag::Gaussian& gaussian)
{
  std::uint64_t hash = 1469598103934665603ull;
  const auto mix = [&hash](std::uint64_t value) { hash = mixHash(hash, value); };
  mix(static_cast<std::uint64_t>(image.size()));
  mixGaussian(hash, gaussian);
  for (const auto& color : image) {
    mix(quantizeFloat(color.r));
    mix(quantizeFloat(color.g));
    mix(quantizeFloat(color.b));
  }
  return hash;
}

std::uint64_t
checksumTerminalInputFingerprint(const std::vector<grabcut_diag::Color>& image,
                                 const grabcut_diag::MixtureGMM& background_gmm,
                                 const grabcut_diag::MixtureGMM& foreground_gmm)
{
  std::uint64_t hash = 1469598103934665603ull;
  const auto mix = [&hash](std::uint64_t value) { hash = mixHash(hash, value); };
  mix(static_cast<std::uint64_t>(image.size()));
  mix(static_cast<std::uint64_t>(background_gmm.components.size()));
  for (const auto& gaussian : background_gmm.components) {
    mixGaussian(hash, gaussian);
  }
  mix(static_cast<std::uint64_t>(foreground_gmm.components.size()));
  for (const auto& gaussian : foreground_gmm.components) {
    mixGaussian(hash, gaussian);
  }
  for (const auto& color : image) {
    mix(quantizeFloat(color.r));
    mix(quantizeFloat(color.g));
    mix(quantizeFloat(color.b));
  }
  return hash;
}

std::uint64_t
checksumComponentAssignments(const std::vector<std::size_t>& components,
                             const std::vector<unsigned char>& foreground_mask)
{
  std::uint64_t hash = 1469598103934665603ull;
  const auto mix = [&hash](std::uint64_t value) { hash = mixHash(hash, value); };
  mix(static_cast<std::uint64_t>(components.size()));
  for (std::size_t i = 0; i < components.size(); ++i) {
    mix(static_cast<std::uint64_t>(foreground_mask[i]));
    mix(static_cast<std::uint64_t>(components[i] + 3));
  }
  return hash;
}

std::uint64_t
checksumFullLearnGMMs(const grabcut_diag::FullLearnGMMsResult& result,
                      const std::vector<unsigned char>& foreground_mask)
{
  std::uint64_t hash = checksumComponentAssignments(result.components, foreground_mask);
  const auto mix = [&hash](std::uint64_t value) { hash = mixHash(hash, value); };
  mix(static_cast<std::uint64_t>(result.background_gmm.components.size()));
  for (const auto& gaussian : result.background_gmm.components) {
    mixGaussian(hash, gaussian);
  }
  mix(static_cast<std::uint64_t>(result.foreground_gmm.components.size()));
  for (const auto& gaussian : result.foreground_gmm.components) {
    mixGaussian(hash, gaussian);
  }
  return hash;
}

std::uint64_t
checksumProductionLearnGMMs(const std::vector<std::size_t>& components,
                            const pcl::segmentation::grabcut::GMM& background_gmm,
                            const pcl::segmentation::grabcut::GMM& foreground_gmm,
                            const std::vector<unsigned char>& foreground_mask)
{
  std::uint64_t hash = checksumComponentAssignments(components, foreground_mask);
  const auto mix = [&hash](std::uint64_t value) { hash = mixHash(hash, value); };
  mix(static_cast<std::uint64_t>(background_gmm.getK()));
  for (std::size_t i = 0; i < background_gmm.getK(); ++i) {
    mixGaussian(hash, background_gmm[i]);
  }
  mix(static_cast<std::uint64_t>(foreground_gmm.getK()));
  for (std::size_t i = 0; i < foreground_gmm.getK(); ++i) {
    mixGaussian(hash, foreground_gmm[i]);
  }
  return hash;
}

struct ErrorStats {
  float max_abs_error = 0.0f;
  float max_rel_error = 0.0f;
  float max_budget_ratio = 0.0f;
};

struct ProfileComponent {
  std::string name;
  double ms = 0.0;
};

struct PublicExtractProfileResult {
  std::vector<ProfileComponent> components;
  std::uint64_t checksum = 0;
  int refine_iterations = 0;
};

struct ProductionLearnGMMState {
  pcl::segmentation::grabcut::GMM background_gmm;
  pcl::segmentation::grabcut::GMM foreground_gmm;
  std::vector<std::size_t> components;
};

ProductionLearnGMMState
makeProductionLearnGMMState(const grabcut_diag::MixtureGMM& background_gmm,
                            const grabcut_diag::MixtureGMM& foreground_gmm,
                            std::size_t sample_count)
{
  ProductionLearnGMMState state;
  copyDiagnosticGMMToProduction(background_gmm, state.background_gmm);
  copyDiagnosticGMMToProduction(foreground_gmm, state.foreground_gmm);
  state.components.assign(sample_count, 0);
  return state;
}

class GrabCutBenchAccess : public pcl::GrabCut<pcl::PointXYZRGB> {
public:
  using pcl::GrabCut<pcl::PointXYZRGB>::GrabCut;

  void
  setPreparedState(const std::vector<grabcut_diag::Color>& colors,
                   const grabcut_diag::MixtureGMM& background_gmm,
                   const grabcut_diag::MixtureGMM& foreground_gmm)
  {
    image_.reset(new pcl::segmentation::grabcut::Image);
    image_->resize(colors.size());
    image_->width = static_cast<std::uint32_t>(colors.size());
    image_->height = 1;
    for (std::size_t i = 0; i < colors.size(); ++i) {
      (*image_)[i].r = colors[i].r;
      (*image_)[i].g = colors[i].g;
      (*image_)[i].b = colors[i].b;
    }

    indices_.reset(new pcl::Indices(colors.size()));
    for (std::size_t i = 0; i < colors.size(); ++i) {
      (*indices_)[i] = static_cast<int>(i);
    }
    trimap_.assign(colors.size(), pcl::segmentation::grabcut::TrimapUnknown);
    hard_segmentation_.assign(colors.size(), pcl::segmentation::grabcut::SegmentationForeground);
    n_links_.assign(colors.size(), NLinks{});

    background_GMM_.resize(background_gmm.components.size());
    foreground_GMM_.resize(foreground_gmm.components.size());
    copyGMM(background_gmm, background_GMM_);
    copyGMM(foreground_gmm, foreground_GMM_);
    computeL();
  }

  void
  runInitGraph()
  {
    initGraph();
  }

  double
  sourceCapacity(std::size_t i) const
  {
    return graph_.getSourceEdgeCapacity(graph_nodes_[i]);
  }

  double
  targetCapacity(std::size_t i) const
  {
    return graph_.getTargetEdgeCapacity(graph_nodes_[i]);
  }

  PublicExtractProfileResult
  runPublicExtractProfile(const PointCloudConstPtr& cloud,
                          const pcl::PointIndicesConstPtr& unknown_indices)
  {
    using namespace pcl::segmentation::grabcut;

    PublicExtractProfileResult result;
    setInputCloud(cloud);

    const auto record = [&](const std::string& name, const auto& fn) {
      const auto start = std::chrono::steady_clock::now();
      fn();
      const auto finish = std::chrono::steady_clock::now();
      const double elapsed_ms =
          std::chrono::duration<double, std::milli>(finish - start).count();
      const auto existing = std::find_if(result.components.begin(),
                                         result.components.end(),
                                         [&](const ProfileComponent& component) {
                                           return component.name == name;
                                         });
      if (existing == result.components.end()) {
        result.components.push_back({name, elapsed_ms});
      }
      else {
        existing->ms += elapsed_ms;
      }
    };

    bool init_ok = false;
    record("base_init_and_validate", [&]() {
      init_ok = pcl::PCLBase<pcl::PointXYZRGB>::initCompute();
      if (init_ok) {
        std::vector<pcl::PCLPointField> in_fields;
        init_ok = (pcl::getFieldIndex<pcl::PointXYZRGB>("rgb", in_fields) != -1) ||
                  (pcl::getFieldIndex<pcl::PointXYZRGB>("rgba", in_fields) != -1);
      }
    });
    if (!init_ok) {
      throw std::runtime_error("public_extract_profile init failed");
    }

    // 这里按 production `initCompute()` 的顺序拆计时。它是 profile（性能剖析）
    // 证据，不替代真实 public entry 的 wall-time 证据。
    record("image_color_staging", [&]() {
      image_.reset(new Image(input_->width, input_->height));
      for (std::size_t i = 0; i < input_->size(); ++i) {
        (*image_)[i] = Color((*input_)[i]);
      }
      width_ = image_->width;
      height_ = image_->height;
    });

    record("state_vector_resize", [&]() {
      if (!tree_ && !input_->isOrganized()) {
        tree_.reset(pcl::search::autoSelectMethod<pcl::PointXYZRGB>(
            input_, true, pcl::search::Purpose::many_knn_search));
      }

      const std::size_t indices_size = indices_->size();
      trimap_ = std::vector<TrimapValue>(indices_size, TrimapUnknown);
      hard_segmentation_ =
          std::vector<SegmentationValue>(indices_size, SegmentationBackground);
      GMM_component_.resize(indices_size);
      n_links_.resize(indices_size);
      foreground_GMM_.resize(K_);
      background_GMM_.resize(K_);
      computeL();
    });

    if (image_->isOrganized()) {
      record("compute_beta_organized", [&]() { computeBetaOrganized(); });
      record("compute_nlinks_organized", [&]() { computeNLinksOrganized(); });
    }
    else {
      record("compute_beta_non_organized", [&]() { computeBetaNonOrganized(); });
      record("compute_nlinks_non_organized", [&]() { computeNLinksNonOrganized(); });
    }

    initialized_ = false;
    record("seed_background_indices", [&]() {
      std::fill(trimap_.begin(), trimap_.end(), TrimapBackground);
      std::fill(hard_segmentation_.begin(),
                hard_segmentation_.end(),
                SegmentationBackground);
      for (const auto& index : unknown_indices->indices) {
        trimap_[index] = TrimapUnknown;
        hard_segmentation_[index] = SegmentationForeground;
      }
    });

    record("build_gmms", [&]() {
      buildGMMs(*image_,
                *indices_,
                hard_segmentation_,
                GMM_component_,
                background_GMM_,
                foreground_GMM_);
    });
    record("initgraph_fit", [&]() { initGraph(); });
    initialized_ = true;

    std::size_t changed = indices_->size();
    while (changed) {
      record("learn_gmms", [&]() {
        learnGMMs(*image_,
                  *indices_,
                  hard_segmentation_,
                  GMM_component_,
                  background_GMM_,
                  foreground_GMM_);
      });
      record("initgraph_refine", [&]() { initGraph(); });
      record("graph_solve", [&]() { static_cast<void>(graph_.solve()); });
      int changed_count = 0;
      record("update_hard_segmentation", [&]() {
        changed_count = updateHardSegmentation();
      });
      changed = static_cast<std::size_t>(changed_count);
      ++result.refine_iterations;
      if (result.refine_iterations > 100) {
        throw std::runtime_error("public_extract_profile exceeded refine guard");
      }
    }

    std::vector<pcl::PointIndices> clusters;
    record("output_clusters", [&]() {
      clusters.clear();
      clusters.resize(2);
      clusters[0].indices.reserve(indices_->size());
      clusters[1].indices.reserve(indices_->size());
      const int indices_size = static_cast<int>(indices_->size());
      for (int i = 0; i < indices_size; ++i) {
        if (hard_segmentation_[i] == SegmentationForeground) {
          clusters[1].indices.push_back(i);
        }
        else {
          clusters[0].indices.push_back(i);
        }
      }
    });
    result.checksum = checksumClusters(clusters);
    return result;
  }

private:
  static void
  copyGMM(const grabcut_diag::MixtureGMM& source, pcl::segmentation::grabcut::GMM& dest)
  {
    for (std::size_t i = 0; i < source.components.size(); ++i) {
      auto& out = dest[i];
      const auto& in = source.components[i];
      out.mu.r = in.mu.r;
      out.mu.g = in.mu.g;
      out.mu.b = in.mu.b;
      out.determinant = in.determinant;
      out.pi = in.pi;
      for (std::size_t row = 0; row < 3; ++row) {
        for (std::size_t col = 0; col < 3; ++col) {
          out.inverse(row, col) = in.inverse(row, col);
        }
      }
    }
  }
};

double
expectedGraphSourceCapacity(float source_capacity, float target_capacity)
{
  double result = 0.0;
  if (source_capacity >= 0.0f) {
    result += source_capacity;
  }
  if (target_capacity < 0.0f) {
    result -= target_capacity;
  }
  return result;
}

double
expectedGraphTargetCapacity(float source_capacity, float target_capacity)
{
  double result = 0.0;
  if (source_capacity < 0.0f) {
    result -= source_capacity;
  }
  if (target_capacity >= 0.0f) {
    result += target_capacity;
  }
  return result;
}

ErrorStats
compareValues(const std::vector<float>& actual,
              const std::vector<float>& expected,
              float error_budget_abs,
              float error_budget_rel)
{
  if (actual.size() != expected.size()) {
    throw std::runtime_error("GMM probability result size mismatch");
  }

  ErrorStats stats;
  for (std::size_t i = 0; i < actual.size(); ++i) {
    const float abs_error = std::abs(actual[i] - expected[i]);
    const float denom = std::max(std::abs(expected[i]), 1.0e-12f);
    const float allowed_error = std::max(error_budget_abs, std::abs(expected[i]) * error_budget_rel);
    stats.max_abs_error = std::max(stats.max_abs_error, abs_error);
    stats.max_rel_error = std::max(stats.max_rel_error, abs_error / denom);
    stats.max_budget_ratio = std::max(stats.max_budget_ratio, abs_error / allowed_error);
  }
  return stats;
}

ErrorStats
compareTerminalWeights(const grabcut_diag::TerminalWeights& actual,
                       const grabcut_diag::TerminalWeights& expected,
                       float error_budget_abs,
                       float error_budget_rel)
{
  ErrorStats foreground =
      compareValues(actual.foreground_costs, expected.foreground_costs, error_budget_abs, error_budget_rel);
  ErrorStats background =
      compareValues(actual.background_costs, expected.background_costs, error_budget_abs, error_budget_rel);
  return {std::max(foreground.max_abs_error, background.max_abs_error),
          std::max(foreground.max_rel_error, background.max_rel_error),
          std::max(foreground.max_budget_ratio, background.max_budget_ratio)};
}

ErrorStats
compareInitGraphNoSolve(const grabcut_diag::InitGraphNoSolveResult& actual,
                        const grabcut_diag::InitGraphNoSolveResult& expected,
                        float error_budget_abs,
                        float error_budget_rel)
{
  ErrorStats stats =
      compareTerminalWeights(actual.terminal, expected.terminal, error_budget_abs, error_budget_rel);
  const float sink_abs_error = std::abs(actual.sink_checksum - expected.sink_checksum);
  const float sink_denom = std::max(std::abs(expected.sink_checksum), 1.0e-12f);
  const float sink_allowed_error =
      std::max(error_budget_abs, std::abs(expected.sink_checksum) * error_budget_rel);
  stats.max_abs_error = std::max(stats.max_abs_error, sink_abs_error);
  stats.max_rel_error = std::max(stats.max_rel_error, sink_abs_error / sink_denom);
  stats.max_budget_ratio = std::max(stats.max_budget_ratio, sink_abs_error / sink_allowed_error);
  return stats;
}

ErrorStats
compareProductionInitGraphTerminal(const GrabCutBenchAccess& grabcut,
                                   const grabcut_diag::TerminalWeights& expected,
                                   float error_budget_abs,
                                   float error_budget_rel)
{
  ErrorStats stats;
  for (std::size_t i = 0; i < expected.foreground_costs.size(); ++i) {
    const double expected_source =
        expectedGraphSourceCapacity(expected.foreground_costs[i], expected.background_costs[i]);
    const double expected_target =
        expectedGraphTargetCapacity(expected.foreground_costs[i], expected.background_costs[i]);
    const double source_abs_error = std::abs(grabcut.sourceCapacity(i) - expected_source);
    const double target_abs_error = std::abs(grabcut.targetCapacity(i) - expected_target);
    const double source_allowed =
        std::max(static_cast<double>(error_budget_abs), std::abs(expected_source) * error_budget_rel);
    const double target_allowed =
        std::max(static_cast<double>(error_budget_abs), std::abs(expected_target) * error_budget_rel);
    stats.max_abs_error =
        std::max(stats.max_abs_error, static_cast<float>(std::max(source_abs_error, target_abs_error)));
    stats.max_rel_error =
        std::max(stats.max_rel_error,
                 static_cast<float>(std::max(source_abs_error / std::max(std::abs(expected_source), 1.0e-12),
                                             target_abs_error / std::max(std::abs(expected_target), 1.0e-12))));
    stats.max_budget_ratio =
        std::max(stats.max_budget_ratio,
                 static_cast<float>(std::max(source_abs_error / source_allowed,
                                             target_abs_error / target_allowed)));
  }
  return stats;
}

double
runOrganizedNLinks(const BenchConfig& config, std::uint64_t& checksum)
{
  const auto image = makeOrganizedImage(config.width, config.height);

  grabcut_diag::OrganizedNLinksResult result;
  for (int i = 0; i < config.warmup_iterations; ++i) {
#ifdef __RVV10__
    result = grabcut_diag::computeOrganizedNLinksRVV(
        image, config.width, config.height, config.lambda);
#else
    result = grabcut_diag::computeOrganizedNLinksReference(
        image, config.width, config.height, config.lambda);
#endif
  }

  const auto start = std::chrono::steady_clock::now();
  for (int i = 0; i < config.iterations; ++i) {
#ifdef __RVV10__
    result = grabcut_diag::computeOrganizedNLinksRVV(
        image, config.width, config.height, config.lambda);
#else
    result = grabcut_diag::computeOrganizedNLinksReference(
        image, config.width, config.height, config.lambda);
#endif
  }
  const auto finish = std::chrono::steady_clock::now();
  checksum = checksumOrganizedNLinks(result);
  return std::chrono::duration<double, std::milli>(finish - start).count() /
         static_cast<double>(config.iterations);
}

double
runGMMProbability(const BenchConfig& config, std::uint64_t& checksum, ErrorStats& error_stats)
{
  const auto image = makeOrganizedImage(config.width, config.height);
  const auto gaussian = makeGaussian();

  std::vector<float> values;
  for (int i = 0; i < config.warmup_iterations; ++i) {
    values = grabcut_diag::computeGMMProbabilityCandidate(gaussian, image);
  }

  const auto start = std::chrono::steady_clock::now();
  for (int i = 0; i < config.iterations; ++i) {
    values = grabcut_diag::computeGMMProbabilityCandidate(gaussian, image);
  }
  const auto finish = std::chrono::steady_clock::now();
  const auto expected = grabcut_diag::computeGMMProbabilityReference(gaussian, image);
  checksum = checksumGMMInputFingerprint(image, gaussian);
  error_stats = compareValues(values, expected, 2.0e-5f, 2.0e-4f);
  return std::chrono::duration<double, std::milli>(finish - start).count() /
         static_cast<double>(config.iterations);
}

double
runTerminalWeights(const BenchConfig& config, std::uint64_t& checksum, ErrorStats& error_stats)
{
  const auto image = makeOrganizedImage(config.width, config.height);
  const auto background_gmm = makeBackgroundGMM();
  const auto foreground_gmm = makeForegroundGMM();

  grabcut_diag::TerminalWeights values;
  for (int i = 0; i < config.warmup_iterations; ++i) {
    values = grabcut_diag::computeTerminalWeightsCandidate(background_gmm, foreground_gmm, image);
  }

  const auto start = std::chrono::steady_clock::now();
  for (int i = 0; i < config.iterations; ++i) {
    values = grabcut_diag::computeTerminalWeightsCandidate(background_gmm, foreground_gmm, image);
  }
  const auto finish = std::chrono::steady_clock::now();
  const auto expected =
      grabcut_diag::computeTerminalWeightsReference(background_gmm, foreground_gmm, image);
  checksum = checksumTerminalInputFingerprint(image, background_gmm, foreground_gmm);
  error_stats = compareTerminalWeights(values, expected, 5.0e-5f, 3.0e-4f);
  return std::chrono::duration<double, std::milli>(finish - start).count() /
         static_cast<double>(config.iterations);
}

double
runInitGraphNoSolve(const BenchConfig& config, std::uint64_t& checksum, ErrorStats& error_stats)
{
  const auto image = makeOrganizedImage(config.width, config.height);
  const auto background_gmm = makeBackgroundGMM();
  const auto foreground_gmm = makeForegroundGMM();

  grabcut_diag::InitGraphNoSolveResult values;
  for (int i = 0; i < config.warmup_iterations; ++i) {
    values = grabcut_diag::computeInitGraphNoSolveCandidate(background_gmm, foreground_gmm, image);
  }

  const auto start = std::chrono::steady_clock::now();
  for (int i = 0; i < config.iterations; ++i) {
    values = grabcut_diag::computeInitGraphNoSolveCandidate(background_gmm, foreground_gmm, image);
  }
  const auto finish = std::chrono::steady_clock::now();
  const auto expected =
      grabcut_diag::computeInitGraphNoSolveReference(background_gmm, foreground_gmm, image);
  checksum = checksumTerminalInputFingerprint(image, background_gmm, foreground_gmm);
  error_stats = compareInitGraphNoSolve(values, expected, 5.0e-5f, 3.0e-4f);
  return std::chrono::duration<double, std::milli>(finish - start).count() /
         static_cast<double>(config.iterations);
}

double
runProductionInitGraphTerminal(const BenchConfig& config,
                               std::uint64_t& checksum,
                               ErrorStats& error_stats)
{
  const auto image = makeOrganizedImage(config.width, config.height);
  const auto background_gmm = makeBackgroundGMM();
  const auto foreground_gmm = makeForegroundGMM();

  GrabCutBenchAccess grabcut(5, config.lambda);
  grabcut.setPreparedState(image, background_gmm, foreground_gmm);
  for (int i = 0; i < config.warmup_iterations; ++i) {
    grabcut.runInitGraph();
  }

  const auto start = std::chrono::steady_clock::now();
  for (int i = 0; i < config.iterations; ++i) {
    grabcut.runInitGraph();
  }
  const auto finish = std::chrono::steady_clock::now();
  const auto expected =
      grabcut_diag::computeTerminalWeightsReference(background_gmm, foreground_gmm, image);
  checksum = checksumTerminalInputFingerprint(image, background_gmm, foreground_gmm);
  error_stats = compareProductionInitGraphTerminal(grabcut, expected, 5.0e-5f, 3.0e-4f);
  return std::chrono::duration<double, std::milli>(finish - start).count() /
         static_cast<double>(config.iterations);
}

double
runLearnGMMAssignment(const BenchConfig& config, std::uint64_t& checksum)
{
  const auto image = makeOrganizedImage(config.width, config.height);
  const auto foreground_mask = makeLearnForegroundMask(config.width, config.height);
  const auto background_gmm = makeBackgroundGMM();
  const auto foreground_gmm = makeForegroundGMM();

  std::vector<std::size_t> components;
  for (int i = 0; i < config.warmup_iterations; ++i) {
    components = grabcut_diag::assignGMMComponentsCandidate(
        background_gmm, foreground_gmm, image, foreground_mask);
  }

  const auto start = std::chrono::steady_clock::now();
  for (int i = 0; i < config.iterations; ++i) {
    components = grabcut_diag::assignGMMComponentsCandidate(
        background_gmm, foreground_gmm, image, foreground_mask);
  }
  const auto finish = std::chrono::steady_clock::now();

  const auto expected = grabcut_diag::assignGMMComponentsReference(
      background_gmm, foreground_gmm, image, foreground_mask);
  if (components != expected) {
    throw std::runtime_error("learn_gmm_assignment component mismatch");
  }
  checksum = checksumComponentAssignments(components, foreground_mask);
  return std::chrono::duration<double, std::milli>(finish - start).count() /
         static_cast<double>(config.iterations);
}

double
runLearnGMMsFull(const BenchConfig& config, std::uint64_t& checksum)
{
  const auto image = makeOrganizedImage(config.width, config.height);
  const auto foreground_mask = makeLearnForegroundMask(config.width, config.height);
  const auto background_gmm = makeBackgroundGMM();
  const auto foreground_gmm = makeForegroundGMM();

  grabcut_diag::FullLearnGMMsResult values;
  for (int i = 0; i < config.warmup_iterations; ++i) {
    values = grabcut_diag::learnGMMsCandidate(
        background_gmm, foreground_gmm, image, foreground_mask);
  }

  const auto start = std::chrono::steady_clock::now();
  for (int i = 0; i < config.iterations; ++i) {
    values = grabcut_diag::learnGMMsCandidate(
        background_gmm, foreground_gmm, image, foreground_mask);
  }
  const auto finish = std::chrono::steady_clock::now();

  const auto expected = grabcut_diag::learnGMMsReference(
      background_gmm, foreground_gmm, image, foreground_mask);
  if (values.components != expected.components ||
      checksumFullLearnGMMs(values, foreground_mask) !=
          checksumFullLearnGMMs(expected, foreground_mask)) {
    throw std::runtime_error("learn_gmms_full result mismatch");
  }
  checksum = checksumFullLearnGMMs(values, foreground_mask);
  return std::chrono::duration<double, std::milli>(finish - start).count() /
         static_cast<double>(config.iterations);
}

double
runProductionLearnGMMs(const BenchConfig& config, std::uint64_t& checksum)
{
  const auto diagnostic_image = makeOrganizedImage(config.width, config.height);
  const auto image = makeProductionImage(diagnostic_image);
  const auto foreground_mask = makeLearnForegroundMask(config.width, config.height);
  const auto hard_segmentation = makeHardSegmentation(foreground_mask);
  const auto background_gmm = makeBackgroundGMM();
  const auto foreground_gmm = makeForegroundGMM();
  pcl::Indices indices(diagnostic_image.size());
  for (std::size_t i = 0; i < diagnostic_image.size(); ++i) {
    indices[i] = static_cast<int>(i);
  }

  for (int i = 0; i < config.warmup_iterations; ++i) {
    auto state =
        makeProductionLearnGMMState(background_gmm, foreground_gmm, diagnostic_image.size());
    pcl::segmentation::grabcut::learnGMMs(image,
                                          indices,
                                          hard_segmentation,
                                          state.components,
                                          state.background_gmm,
                                          state.foreground_gmm);
    checksum =
        checksumProductionLearnGMMs(state.components,
                                    state.background_gmm,
                                    state.foreground_gmm,
                                    foreground_mask);
  }

  std::vector<ProductionLearnGMMState> states;
  states.reserve(static_cast<std::size_t>(config.iterations));
  for (int i = 0; i < config.iterations; ++i) {
    states.push_back(
        makeProductionLearnGMMState(background_gmm, foreground_gmm, diagnostic_image.size()));
  }

  const auto start = std::chrono::steady_clock::now();
  for (auto& state : states) {
    pcl::segmentation::grabcut::learnGMMs(image,
                                          indices,
                                          hard_segmentation,
                                          state.components,
                                          state.background_gmm,
                                          state.foreground_gmm);
  }
  const auto finish = std::chrono::steady_clock::now();

  const auto expected = grabcut_diag::learnGMMsReference(
      background_gmm, foreground_gmm, diagnostic_image, foreground_mask);
  if (states.back().components != expected.components) {
    throw std::runtime_error("production_learn_gmms component mismatch");
  }
  checksum = checksumProductionLearnGMMs(states.back().components,
                                        states.back().background_gmm,
                                        states.back().foreground_gmm,
                                        foreground_mask);
  return std::chrono::duration<double, std::milli>(finish - start).count() /
         static_cast<double>(config.iterations);
}

double
runPublicExtract(const BenchConfig& config, std::uint64_t& checksum)
{
  const auto image = makeOrganizedImage(config.width, config.height);
  const auto cloud = makeOrganizedCloud(image, config.width, config.height);
  const auto unknown_indices = makeCentralUnknownIndices(config.width, config.height);

  const auto run_once = [&]() {
    pcl::GrabCut<pcl::PointXYZRGB> grabcut(5, config.lambda);
    grabcut.setInputCloud(cloud);
    grabcut.setBackgroundPointsIndices(unknown_indices);
    std::vector<pcl::PointIndices> clusters;
    grabcut.extract(clusters);
    return checksumClusters(clusters);
  };

  for (int i = 0; i < config.warmup_iterations; ++i) {
    checksum = run_once();
  }

  const auto start = std::chrono::steady_clock::now();
  for (int i = 0; i < config.iterations; ++i) {
    checksum = run_once();
  }
  const auto finish = std::chrono::steady_clock::now();
  return std::chrono::duration<double, std::milli>(finish - start).count() /
         static_cast<double>(config.iterations);
}

std::vector<ProfileComponent>
runPublicExtractProfile(const BenchConfig& config,
                        std::uint64_t& checksum,
                        int& refine_iterations,
                        double& total_ms)
{
  const auto image = makeOrganizedImage(config.width, config.height);
  const auto cloud = makeOrganizedCloud(image, config.width, config.height);
  const auto unknown_indices = makeCentralUnknownIndices(config.width, config.height);

  const auto run_once = [&]() {
    GrabCutBenchAccess grabcut(5, config.lambda);
    return grabcut.runPublicExtractProfile(cloud, unknown_indices);
  };

  for (int i = 0; i < config.warmup_iterations; ++i) {
    const auto warmup = run_once();
    checksum = warmup.checksum;
  }

  std::vector<ProfileComponent> totals;
  for (int i = 0; i < config.iterations; ++i) {
    const auto profile = run_once();
    checksum = profile.checksum;
    refine_iterations = profile.refine_iterations;
    if (totals.empty()) {
      totals = profile.components;
    }
    else {
      if (totals.size() != profile.components.size()) {
        throw std::runtime_error("public_extract_profile component count changed");
      }
      for (std::size_t idx = 0; idx < totals.size(); ++idx) {
        if (totals[idx].name != profile.components[idx].name) {
          throw std::runtime_error("public_extract_profile component order changed");
        }
        totals[idx].ms += profile.components[idx].ms;
      }
    }
  }

  total_ms = 0.0;
  for (auto& component : totals) {
    component.ms /= static_cast<double>(config.iterations);
    total_ms += component.ms;
  }
  return totals;
}

void
printBenchLine(const BenchConfig& config,
               const std::string& case_name,
               double avg_ms,
               std::uint64_t checksum)
{
  std::cout << case_name << ": " << std::fixed << std::setprecision(6) << avg_ms
            << " ms / iter\n";
  std::cout << "BENCH grabcut_component"
            << " case=" << case_name
#ifdef __RVV10__
            << " path=rvv_candidate"
#else
            << " path=reference"
#endif
            << " width=" << config.width
            << " height=" << config.height
            << " iterations=" << config.iterations
            << " warmup=" << config.warmup_iterations
            << " avg_ms=" << std::fixed << std::setprecision(6) << avg_ms
            << " checksum_policy="
            << (case_name == "gmm_probability" ? "input_fingerprint_error_budget"
                                                : "output_quantized_exact")
            << " checksum=" << checksum
            << '\n';
}

void
printPublicExtractProfileBenchLines(const BenchConfig& config,
                                    const std::vector<ProfileComponent>& components,
                                    double total_ms,
                                    std::uint64_t checksum,
                                    int refine_iterations)
{
  std::cout << "public_extract_profile: " << std::fixed << std::setprecision(6)
            << total_ms << " ms / iter\n";
  std::cout << "BENCH grabcut_component"
            << " case=public_extract_profile"
#ifdef __RVV10__
            << " path=rvv_candidate"
#else
            << " path=reference"
#endif
            << " width=" << config.width
            << " height=" << config.height
            << " iterations=" << config.iterations
            << " warmup=" << config.warmup_iterations
            << " avg_ms=" << std::fixed << std::setprecision(6) << total_ms
            << " checksum_policy=output_quantized_exact"
            << " checksum=" << checksum
            << " refine_iterations=" << refine_iterations
            << '\n';
  for (const auto& component : components) {
    const double pct = total_ms > 0.0 ? component.ms * 100.0 / total_ms : 0.0;
    std::cout << "BENCH grabcut_profile_component"
              << " case=public_extract_profile"
              << " component=" << component.name
#ifdef __RVV10__
              << " path=rvv_candidate"
#else
              << " path=reference"
#endif
              << " width=" << config.width
              << " height=" << config.height
              << " iterations=" << config.iterations
              << " warmup=" << config.warmup_iterations
              << " avg_ms=" << std::fixed << std::setprecision(6) << component.ms
              << " pct=" << pct
              << " checksum=" << checksum
              << " refine_iterations=" << refine_iterations
              << '\n';
  }
}

void
printGMMBenchLine(const BenchConfig& config,
                  double avg_ms,
                  std::uint64_t checksum,
                  const ErrorStats& error_stats)
{
  std::cout << "gmm_probability: " << std::fixed << std::setprecision(6) << avg_ms
            << " ms / iter\n";
  std::cout << "BENCH grabcut_component"
            << " case=gmm_probability"
#ifdef __RVV10__
            << " path=rvv_candidate"
#else
            << " path=reference"
#endif
            << " width=" << config.width
            << " height=" << config.height
            << " iterations=" << config.iterations
            << " warmup=" << config.warmup_iterations
            << " avg_ms=" << std::fixed << std::setprecision(6) << avg_ms
            << " checksum_policy=input_fingerprint_error_budget"
            << " checksum=" << checksum
            << " max_abs_error=" << std::scientific << std::setprecision(6)
            << error_stats.max_abs_error
            << " max_rel_error=" << error_stats.max_rel_error
            << " error_budget_abs=2.000000e-05"
            << " error_budget_rel=2.000000e-04"
            << " max_budget_ratio=" << error_stats.max_budget_ratio
            << std::fixed
            << '\n';
}

void
printErrorBudgetBenchLine(const BenchConfig& config,
                          const std::string& case_name,
                          double avg_ms,
                          std::uint64_t checksum,
                          const ErrorStats& error_stats,
                          float error_budget_abs,
                          float error_budget_rel)
{
  std::cout << case_name << ": " << std::fixed << std::setprecision(6) << avg_ms
            << " ms / iter\n";
  std::cout << "BENCH grabcut_component"
            << " case=" << case_name
#ifdef __RVV10__
            << " path=rvv_candidate"
#else
            << " path=reference"
#endif
            << " width=" << config.width
            << " height=" << config.height
            << " iterations=" << config.iterations
            << " warmup=" << config.warmup_iterations
            << " avg_ms=" << std::fixed << std::setprecision(6) << avg_ms
            << " checksum_policy=input_fingerprint_error_budget"
            << " checksum=" << checksum
            << " max_abs_error=" << std::scientific << std::setprecision(6)
            << error_stats.max_abs_error
            << " max_rel_error=" << error_stats.max_rel_error
            << " error_budget_abs=" << error_budget_abs
            << " error_budget_rel=" << error_budget_rel
            << " max_budget_ratio=" << error_stats.max_budget_ratio
            << std::fixed
            << '\n';
}

} // namespace

int
main(int argc, char** argv)
{
  const auto config = parseArgs(argc, argv);
  pcl::console::setVerbosityLevel(pcl::console::L_ERROR);

  std::cout << "Dataset: synthetic GrabCut organized RGB image width=" << config.width
            << " height=" << config.height << '\n';
  std::cout << "Iterations: " << config.iterations << '\n';
  std::cout << "Warmup Iterations: " << config.warmup_iterations << '\n';
  std::cout << "Case: " << config.bench_case << '\n';

  if (config.bench_case == "all" || config.bench_case == "organized_nlinks") {
    std::uint64_t checksum = 0;
    const double avg_ms = runOrganizedNLinks(config, checksum);
    printBenchLine(config, "organized_nlinks", avg_ms, checksum);
  }
  if (config.bench_case == "all" || config.bench_case == "gmm_probability") {
    std::uint64_t checksum = 0;
    ErrorStats error_stats;
    const double avg_ms = runGMMProbability(config, checksum, error_stats);
    printErrorBudgetBenchLine(
        config, "gmm_probability", avg_ms, checksum, error_stats, 2.0e-5f, 2.0e-4f);
  }
  if (config.bench_case == "all" || config.bench_case == "terminal_weights") {
    std::uint64_t checksum = 0;
    ErrorStats error_stats;
    const double avg_ms = runTerminalWeights(config, checksum, error_stats);
    printErrorBudgetBenchLine(
        config, "terminal_weights", avg_ms, checksum, error_stats, 5.0e-5f, 3.0e-4f);
  }
  if (config.bench_case == "all" || config.bench_case == "initgraph_no_solve") {
    std::uint64_t checksum = 0;
    ErrorStats error_stats;
    const double avg_ms = runInitGraphNoSolve(config, checksum, error_stats);
    printErrorBudgetBenchLine(
        config, "initgraph_no_solve", avg_ms, checksum, error_stats, 5.0e-5f, 3.0e-4f);
  }
  if (config.bench_case == "all" || config.bench_case == "production_initgraph_terminal") {
    std::uint64_t checksum = 0;
    ErrorStats error_stats;
    const double avg_ms = runProductionInitGraphTerminal(config, checksum, error_stats);
    printErrorBudgetBenchLine(config,
                              "production_initgraph_terminal",
                              avg_ms,
                              checksum,
                              error_stats,
                              5.0e-5f,
                              3.0e-4f);
  }
  if (config.bench_case == "all" || config.bench_case == "learn_gmm_assignment") {
    std::uint64_t checksum = 0;
    const double avg_ms = runLearnGMMAssignment(config, checksum);
    printBenchLine(config, "learn_gmm_assignment", avg_ms, checksum);
  }
  if (config.bench_case == "all" || config.bench_case == "learn_gmms_full") {
    std::uint64_t checksum = 0;
    const double avg_ms = runLearnGMMsFull(config, checksum);
    printBenchLine(config, "learn_gmms_full", avg_ms, checksum);
  }
  if (config.bench_case == "all" || config.bench_case == "production_learn_gmms") {
    std::uint64_t checksum = 0;
    const double avg_ms = runProductionLearnGMMs(config, checksum);
    printBenchLine(config, "production_learn_gmms", avg_ms, checksum);
  }
  if (config.bench_case == "all" || config.bench_case == "public_extract") {
    std::uint64_t checksum = 0;
    const double avg_ms = runPublicExtract(config, checksum);
    printBenchLine(config, "public_extract", avg_ms, checksum);
  }
  if (config.bench_case == "all" || config.bench_case == "public_extract_profile") {
    std::uint64_t checksum = 0;
    int refine_iterations = 0;
    double total_ms = 0.0;
    const auto components =
        runPublicExtractProfile(config, checksum, refine_iterations, total_ms);
    printPublicExtractProfileBenchLines(
        config, components, total_ms, checksum, refine_iterations);
  }
  return 0;
}
