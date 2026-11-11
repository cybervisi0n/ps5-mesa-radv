#!/bin/bash
./download_dependencies.sh
./ps5-scripts/build_clc.sh
./ps5-scripts/setup_build_ps5_debug.sh
./ps5-scripts/build_ps5_debug.sh
./ps5-scripts/package_ps5_libs.sh build_debug ps5-mesa_debug
