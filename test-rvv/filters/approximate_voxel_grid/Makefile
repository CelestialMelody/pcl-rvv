# =============================================================================
#  PCL filters/approximate_voxel_grid RVV production + diagnostic Makefile
# =============================================================================

TOPIC  := approximate_voxel_grid
MODULE := filters

FOCUS_DIR      := pcl/filters/include/pcl/filters/impl/approximate_voxel_grid.hpp
FILTER_REPORT  := log/filtered_approximate_voxel_grid.log
ANALYZE_REPORT := log/analyze_approximate_voxel_grid.log

TARGET_BENCH     := bench_approximate_voxel_grid
TARGET_BENCH_STD := bench_approximate_voxel_grid_std
TARGET_BENCH_RVV := bench_approximate_voxel_grid_rvv
SRCS_BENCH       := bench_approximate_voxel_grid.cpp

TARGET_TEST     := test_approximate_voxel_grid
TARGET_TEST_STD := test_approximate_voxel_grid_std
TARGET_TEST_RVV := test_approximate_voxel_grid_rvv
SRCS_TEST       := test_approximate_voxel_grid.cpp

EXTRA_CXXFLAGS := -DPCL_APPROXIMATE_VOXEL_GRID_RVV_DIAGNOSTIC
LIBS_BENCH := -lpcl_filters -lpcl_common -lm
LIBS_TEST  := -lpcl_filters -lpcl_common -lgtest -lgtest_main -lpthread -lm

include ../../mk/rvv-topic.mk
