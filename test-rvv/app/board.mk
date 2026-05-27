# =============================================================================
# 板卡侧运行（与开发机 deploy_* / deploy_board 配合）
#
# 用法（板卡上，在含本文件的目录）：
#   export LD_LIBRARY_PATH=/root/pcl-test/lib:$LD_LIBRARY_PATH
#   make -f board.mk run_pipeline_rvv
#   make -f board.mk run_dag_rvv
#   make -f board.mk run_hot_rvv           # pcl_pipeline_hot（仅 common 热区对比）
#   make -f board.mk run_pipeline_compare  # std + rvv 跑完并生成对比报告
#   make -f board.mk run_hot_compare
#
# 可执行需已 rsync 到 REMOTE_DIR：pipeline_{app,dag}_{std,rvv}[, pipeline_hot_{std,rvv}]
# 部署：开发机  make deploy_hot_std deploy_hot_rvv REMOTE_IP=...
#       deploy_read_vlen REMOTE_IP=...（CSR vlenb → VLEN）
#       或与脚本一起：make deploy_compare_script REMOTE_IP=...
#   analyze_bench_compare.py -> $(REMOTE_DIR)/../script/（与下方 SCRIPT_DIR 一致）
# =============================================================================

REMOTE_DIR         ?= /root/pcl-test/app

# 板卡展示名（可覆盖）；read_vlen 默认与 app 二进制同目录
BOARD_LABEL   ?= Milkv-Jupiter
READ_VLEN      ?= $(REMOTE_DIR)/read_vlen
BOARD_DEVICE   ?= board rv64 ($(BOARD_LABEL))
ifndef BOARD_VLEN_DESC
BOARD_VLEN_DESC := $(shell test -x '$(READ_VLEN)' && '$(READ_VLEN)' 2>/dev/null || echo 'n/a (deploy read_vlen to $(READ_VLEN) or: gcc -O2 -o read_vlen path/to/riscv_read_vlen.c)')
endif

REMOTE_LIB_DIR     ?= /root/pcl-test/lib
REMOTE_OUTPUT_DIR  = $(REMOTE_DIR)/output

REMOTE_PIPELINE_STD = pipeline_app_std
REMOTE_PIPELINE_RVV = pipeline_app_rvv
REMOTE_DAG_STD      = pipeline_dag_std
REMOTE_DAG_RVV      = pipeline_dag_rvv
REMOTE_HOT_STD      = pipeline_hot_std
REMOTE_HOT_RVV      = pipeline_hot_rvv

PIPELINE_STD_LOG = $(REMOTE_OUTPUT_DIR)/run_pipeline_std.log
PIPELINE_RVV_LOG = $(REMOTE_OUTPUT_DIR)/run_pipeline_rvv.log
DAG_STD_LOG      = $(REMOTE_OUTPUT_DIR)/run_dag_std.log
DAG_RVV_LOG      = $(REMOTE_OUTPUT_DIR)/run_dag_rvv.log
HOT_STD_LOG      = $(REMOTE_OUTPUT_DIR)/run_hot_std.log
HOT_RVV_LOG      = $(REMOTE_OUTPUT_DIR)/run_hot_rvv.log

# 默认：脚本在 app 上一级目录的 script/（勿依赖 makefile 路径；与 deploy_compare_script 远端布局一致）
SCRIPT_DIR ?= $(abspath $(REMOTE_DIR)/../script)
PYTHON ?= python3
BENCH_COMPARE_SCRIPT = $(SCRIPT_DIR)/analyze_bench_compare.py

PIPELINE_COMPARE_REPORT = $(REMOTE_OUTPUT_DIR)/analyze_pipeline_compare.log
DAG_COMPARE_REPORT      = $(REMOTE_OUTPUT_DIR)/analyze_dag_compare.log
HOT_COMPARE_REPORT      = $(REMOTE_OUTPUT_DIR)/analyze_hot_compare.log

$(REMOTE_OUTPUT_DIR):
	mkdir -p $@

# 传给可执行文件的附加参数。-v/--verbose 才会在 stderr 打出「首次迭代阶段」说明（与主 Makefile 一致）。
# 默认 pipeline 附带 -v；DAG / hot 默认可为空（可自行 BOARD_*_ARGS=-v）。
BOARD_PIPELINE_ARGS ?= -v
BOARD_DAG_ARGS      ?=
BOARD_HOT_ARGS      ?=

run_pipeline_std:
	@test -x "$(REMOTE_DIR)/$(REMOTE_PIPELINE_STD)" || (echo "[BOARD] missing $(REMOTE_DIR)/$(REMOTE_PIPELINE_STD)" >&2; exit 1)
	@mkdir -p $(REMOTE_OUTPUT_DIR)
	LD_LIBRARY_PATH=$(REMOTE_LIB_DIR):$$LD_LIBRARY_PATH \
	$(REMOTE_DIR)/$(REMOTE_PIPELINE_STD) $(BOARD_PIPELINE_ARGS) 2>&1 | tee $(PIPELINE_STD_LOG)

run_pipeline_rvv:
	@test -x "$(REMOTE_DIR)/$(REMOTE_PIPELINE_RVV)" || (echo "[BOARD] missing $(REMOTE_DIR)/$(REMOTE_PIPELINE_RVV)" >&2; exit 1)
	@mkdir -p $(REMOTE_OUTPUT_DIR)
	LD_LIBRARY_PATH=$(REMOTE_LIB_DIR):$$LD_LIBRARY_PATH \
	$(REMOTE_DIR)/$(REMOTE_PIPELINE_RVV) $(BOARD_PIPELINE_ARGS) 2>&1 | tee $(PIPELINE_RVV_LOG)

run_dag_std:
	@test -x "$(REMOTE_DIR)/$(REMOTE_DAG_STD)" || (echo "[BOARD] missing $(REMOTE_DIR)/$(REMOTE_DAG_STD)" >&2; exit 1)
	@mkdir -p $(REMOTE_OUTPUT_DIR)
	LD_LIBRARY_PATH=$(REMOTE_LIB_DIR):$$LD_LIBRARY_PATH \
	$(REMOTE_DIR)/$(REMOTE_DAG_STD) $(BOARD_DAG_ARGS) 2>&1 | tee $(DAG_STD_LOG)

run_dag_rvv:
	@test -x "$(REMOTE_DIR)/$(REMOTE_DAG_RVV)" || (echo "[BOARD] missing $(REMOTE_DIR)/$(REMOTE_DAG_RVV)" >&2; exit 1)
	@mkdir -p $(REMOTE_OUTPUT_DIR)
	LD_LIBRARY_PATH=$(REMOTE_LIB_DIR):$$LD_LIBRARY_PATH \
	$(REMOTE_DIR)/$(REMOTE_DAG_RVV) $(BOARD_DAG_ARGS) 2>&1 | tee $(DAG_RVV_LOG)

run_hot_std:
	@test -x "$(REMOTE_DIR)/$(REMOTE_HOT_STD)" || (echo "[BOARD] missing $(REMOTE_DIR)/$(REMOTE_HOT_STD)" >&2; exit 1)
	@mkdir -p $(REMOTE_OUTPUT_DIR)
	LD_LIBRARY_PATH=$(REMOTE_LIB_DIR):$$LD_LIBRARY_PATH \
	$(REMOTE_DIR)/$(REMOTE_HOT_STD) $(BOARD_HOT_ARGS) 2>&1 | tee $(HOT_STD_LOG)

run_hot_rvv:
	@test -x "$(REMOTE_DIR)/$(REMOTE_HOT_RVV)" || (echo "[BOARD] missing $(REMOTE_DIR)/$(REMOTE_HOT_RVV)" >&2; exit 1)
	@mkdir -p $(REMOTE_OUTPUT_DIR)
	LD_LIBRARY_PATH=$(REMOTE_LIB_DIR):$$LD_LIBRARY_PATH \
	$(REMOTE_DIR)/$(REMOTE_HOT_RVV) $(BOARD_HOT_ARGS) 2>&1 | tee $(HOT_RVV_LOG)

# 依次跑 std/rvv、tee，再离线对比（需在板卡上装好 python3）
run_pipeline_compare: | $(REMOTE_OUTPUT_DIR)
	@$(MAKE) -f $(lastword $(MAKEFILE_LIST)) run_pipeline_std
	@$(MAKE) -f $(lastword $(MAKEFILE_LIST)) run_pipeline_rvv
	$(PYTHON) $(BENCH_COMPARE_SCRIPT) \
		--std-log $(PIPELINE_STD_LOG) \
		--rvv-log $(PIPELINE_RVV_LOG) \
		--device "$(BOARD_DEVICE)" \
		--vlen-desc "$(BOARD_VLEN_DESC)" \
		2>&1 | tee $(PIPELINE_COMPARE_REPORT)

run_dag_compare: | $(REMOTE_OUTPUT_DIR)
	@$(MAKE) -f $(lastword $(MAKEFILE_LIST)) run_dag_std
	@$(MAKE) -f $(lastword $(MAKEFILE_LIST)) run_dag_rvv
	$(PYTHON) $(BENCH_COMPARE_SCRIPT) \
		--std-log $(DAG_STD_LOG) \
		--rvv-log $(DAG_RVV_LOG) \
		--device "$(BOARD_DEVICE)" \
		--vlen-desc "$(BOARD_VLEN_DESC)" \
		2>&1 | tee $(DAG_COMPARE_REPORT)

run_hot_compare: | $(REMOTE_OUTPUT_DIR)
	@$(MAKE) -f $(lastword $(MAKEFILE_LIST)) run_hot_std
	@$(MAKE) -f $(lastword $(MAKEFILE_LIST)) run_hot_rvv
	$(PYTHON) $(BENCH_COMPARE_SCRIPT) \
		--std-log $(HOT_STD_LOG) \
		--rvv-log $(HOT_RVV_LOG) \
		--device "$(BOARD_DEVICE)" \
		--vlen-desc "$(BOARD_VLEN_DESC)" \
		2>&1 | tee $(HOT_COMPARE_REPORT)

# 已有两份 tee 日志时只做对比（不写板卡二进制）
analyze_pipeline_compare: $(PIPELINE_STD_LOG) $(PIPELINE_RVV_LOG) | $(REMOTE_OUTPUT_DIR)
	$(PYTHON) $(BENCH_COMPARE_SCRIPT) \
		--std-log $(PIPELINE_STD_LOG) \
		--rvv-log $(PIPELINE_RVV_LOG) \
		--device "$(BOARD_DEVICE)" \
		--vlen-desc "$(BOARD_VLEN_DESC)" \
		2>&1 | tee $(PIPELINE_COMPARE_REPORT)

analyze_dag_compare: $(DAG_STD_LOG) $(DAG_RVV_LOG) | $(REMOTE_OUTPUT_DIR)
	$(PYTHON) $(BENCH_COMPARE_SCRIPT) \
		--std-log $(DAG_STD_LOG) \
		--rvv-log $(DAG_RVV_LOG) \
		--device "$(BOARD_DEVICE)" \
		--vlen-desc "$(BOARD_VLEN_DESC)" \
		2>&1 | tee $(DAG_COMPARE_REPORT)

analyze_hot_compare: $(HOT_STD_LOG) $(HOT_RVV_LOG) | $(REMOTE_OUTPUT_DIR)
	$(PYTHON) $(BENCH_COMPARE_SCRIPT) \
		--std-log $(HOT_STD_LOG) \
		--rvv-log $(HOT_RVV_LOG) \
		--device "$(BOARD_DEVICE)" \
		--vlen-desc "$(BOARD_VLEN_DESC)" \
		2>&1 | tee $(HOT_COMPARE_REPORT)

.PHONY: run_pipeline_std run_pipeline_rvv run_dag_std run_dag_rvv run_hot_std run_hot_rvv \
	run_pipeline_compare run_dag_compare run_hot_compare \
	analyze_pipeline_compare analyze_dag_compare analyze_hot_compare
