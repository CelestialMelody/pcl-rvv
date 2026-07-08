# =============================================================================
#  PCL filters/voxel_grid_covariance RVV diagnostic Makefile
#
# Topic-specific parameters live here; shared build, QEMU and board deploy rules
# are provided by test-rvv/mk/rvv-topic.mk.
# =============================================================================

TOPIC  := voxel_grid_covariance
MODULE := filters

FOCUS_DIR      := pcl/filters/include/pcl/filters/impl/voxel_grid_covariance.hpp
FILTER_REPORT  := log/filtered_voxel_grid_covariance.log
ANALYZE_REPORT := log/analyze_voxel_grid_covariance.log

TARGET_BENCH     := bench_voxel_grid_covariance
TARGET_BENCH_STD := bench_voxel_grid_covariance_std
TARGET_BENCH_RVV := bench_voxel_grid_covariance_rvv
SRCS_BENCH       := bench_voxel_grid_covariance.cpp

TARGET_TEST     := test_voxel_grid_covariance
TARGET_TEST_STD := test_voxel_grid_covariance_std
TARGET_TEST_RVV := test_voxel_grid_covariance_rvv
SRCS_TEST       := test_voxel_grid_covariance.cpp

TARGET_UPSTREAM_TEST     := test_voxel_grid_covariance_upstream
TARGET_UPSTREAM_TEST_STD := test_voxel_grid_covariance_upstream_std
TARGET_UPSTREAM_TEST_RVV := test_voxel_grid_covariance_upstream_rvv
SRCS_UPSTREAM_TEST       = $(PCL_SOURCE_ROOT)/test/filters/test_filters.cpp
UPSTREAM_TEST_ARGS       = $(PCL_SOURCE_ROOT)/test/bun0.pcd $(PCL_SOURCE_ROOT)/test/milk_cartoon_all_small_clorox.pcd --gtest_filter=VoxelGridCovariance.Filters

LIBS_CORE := \
	-lpcl_search \
	-lpcl_kdtree \
	-lpcl_octree \
	-lpcl_common \
	-lflann_cpp \
	-lboost_filesystem -lboost_iostreams -lboost_system \
	-lz -lhdf5 -llz4 -lpng -lm
LIBS_BENCH := $(LIBS_CORE)
LIBS_TEST  := $(LIBS_CORE) -lgtest -lgtest_main -lpthread
LIBS_UPSTREAM_TEST := \
	-lpcl_filters \
	-lpcl_io \
	-lpcl_sample_consensus \
	-lpcl_search \
	-lpcl_kdtree \
	-lpcl_octree \
	-lpcl_common \
	-lflann_cpp \
	-lboost_filesystem -lboost_iostreams -lboost_system \
	-lgtest -lgtest_main -lpthread \
	-lz -lhdf5 -llz4 -lpng -lm

include ../../mk/rvv-topic.mk
