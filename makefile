# Generic Makefile

BUILD_DIR=build
PLUGIN_DIR=plugins
TOOL_DIR=tools
SHELL := /bin/bash
include ${BUILD_DIR}/base.mk
-include ${PLUGIN_DIR}/*/plugin.mk
-include ${TOOL_DIR}/*/tool.mk
