# =============================================================================
#  PCL common/distances RVV diagnostic Makefile
#
# Topic-specific parameters live here; shared build, QEMU and board deploy rules
# are provided by test-rvv/mk/rvv-topic.mk. The getMaxSegment load-compare bench
# remains a distances-specific extension.
# =============================================================================

TOPIC  := distances
MODULE := common

FOCUS_DIR      := pcl/common/include/pcl/common/distances.h
FILTER_REPORT  := log/filtered_distances.log
ANALYZE_REPORT := log/analyze_distances.log

TARGET_BENCH     := bench_distances
TARGET_BENCH_STD := bench_distances_std
TARGET_BENCH_RVV := bench_distances_rvv
SRCS_BENCH       := bench_distances.cpp

TARGET_TEST     := test_distances
TARGET_TEST_STD := test_distances_std
TARGET_TEST_RVV := test_distances_rvv
BOARD_TARGET_TEST := test_distances
SRCS_TEST       := test_distances.cpp

TARGET_BENCH_LOAD ?= bench_getmaxsegment_load_compare
SRCS_BENCH_LOAD   := bench_getmaxsegment_load_compare.cpp

REMOTE_DIR ?= ~/pcl-test/common/distances

LIBS_BENCH := -lpcl_common -lm
LIBS_TEST  := -lpcl_common -lgtest -lgtest_main -lpthread -lm

include ../../mk/rvv-topic.mk

TARGET_BENCH_LOAD_BIN := $(BUILD_DIR)/$(ARCH)/$(TARGET_BENCH_LOAD)

$(TARGET_BENCH_LOAD_BIN): $(SRCS_BENCH_LOAD) | $(BUILD_DIR)/$(ARCH)
	@echo "[BUILD] Compiling getMaxSegment load-compare bench ($(ARCH))..."
	$(CXX) $(CXXFLAGS) $(SRCS_BENCH_LOAD) $(LDFLAGS) $(EXTRA_LDFLAGS) $(LIBS_BENCH) -o $(TARGET_BENCH_LOAD_BIN)

run_bench_load_compare: clean_bench_load_compare $(TARGET_BENCH_LOAD_BIN)
	@echo "[RUN] getMaxSegment load-compare on $(ARCH) (USE_PCL_RVV10=$(USE_PCL_RVV10))..."
	$(RUN_CMD) ./$(TARGET_BENCH_LOAD_BIN) 2500 20 3

clean_bench_load_compare:
	rm -f $(TARGET_BENCH_LOAD_BIN)

deploy_bench_load_compare: deploy_files clean_bench_load_compare
	@$(MAKE) -C $(CURDIR) USE_PCL_RVV10=1 TARGET_BENCH_LOAD=$(TARGET_BENCH_LOAD) $(BUILD_DIR)/$(ARCH)/$(TARGET_BENCH_LOAD)
	@$(STRIP) -s ./$(BUILD_DIR)/$(ARCH)/$(TARGET_BENCH_LOAD) -o ./$(TARGET_BENCH_LOAD)_stripped
	@rsync -e "$(RSYNC_SSH)" -avzP ./$(TARGET_BENCH_LOAD)_stripped $(REMOTE_USER)@$(REMOTE_IP):$(REMOTE_DIR)/$(TARGET_BENCH_LOAD)
	@rm -f ./$(TARGET_BENCH_LOAD)_stripped

clean: clean_bench_load_compare

.PHONY: run_bench_load_compare clean_bench_load_compare deploy_bench_load_compare
