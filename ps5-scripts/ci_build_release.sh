#!/bin/bash
./download_dependencies.sh
./ps5-scripts/setup_build_ps5_release.sh
./ps5-scripts/build_ps5_release.sh
./ps5-scripts/package_ps5_libs.sh build_release ps5-mesa_release
