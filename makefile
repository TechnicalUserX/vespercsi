# Generic Makefile

BUILD_DIR=build
PLUGIN_DIR=plugins
SHELL := /bin/bash
include ${BUILD_DIR}/base.mk
-include ${PLUGIN_DIR}/*/plugin.mk