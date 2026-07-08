# =============================================================================
#  PCL filters/plane_clipper3D RVV diagnostic Makefile
#
# Topic-specific parameters live here; shared build, QEMU and board deploy rules
# are provided by test-rvv/mk/rvv-topic.mk.
# =============================================================================

TOPIC  := plane_clipper3D
MODULE := filters

FOCUS_DIR      := pcl/filters/include/pcl/filters/impl/plane_clipper3D.hpp
FILTER_REPORT  := log/filtered_plane_clipper3D.log
ANALYZE_REPORT := log/analyze_plane_clipper3D.log

TARGET_BENCH     := bench_plane_clipper3D
TARGET_BENCH_STD := bench_plane_clipper3D_std
TARGET_BENCH_RVV := bench_plane_clipper3D_rvv
SRCS_BENCH       := bench_plane_clipper3D.cpp

TARGET_TEST     := test_plane_clipper3D
TARGET_TEST_STD := test_plane_clipper3D_std
TARGET_TEST_RVV := test_plane_clipper3D_rvv
SRCS_TEST       := test_plane_clipper3D.cpp

TARGET_UPSTREAM_TEST     := test_plane_clipper3D_upstream
TARGET_UPSTREAM_TEST_STD := test_plane_clipper3D_upstream_std
TARGET_UPSTREAM_TEST_RVV := test_plane_clipper3D_upstream_rvv
SRCS_UPSTREAM_TEST       = $(PCL_SOURCE_ROOT)/test/filters/test_clipper.cpp
UPSTREAM_TEST_ARGS       :=

LIBS_BENCH := -lpcl_common -lm
LIBS_TEST  := -lpcl_common -lgtest -lgtest_main -lpthread -lm
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
