#!/bin/bash

ninja -C "build_debug" \
    src/util/blake3/libblake3.a \
    src/c11/impl/libmesa_util_c11.a \
    src/util/libmesa_util_simd.a \
	src/util/libmesa_util_clflush.a \
	src/util/libmesa_util_clflushopt.a \
    src/util/libmesa_util.a \
    src/compiler/libcompiler.a \
    src/compiler/nir/libnir.a \
    src/compiler/spirv/libvtn.a \
    src/amd/vulkan/libvulkan_radeon.a \
    src/mesa/libmesa.a \
    src/mesa/libmesa_sse41.a \
    src/compiler/glsl/libglsl_util.a \
    src/compiler/glsl/glcpp/libglcpp.a \
    src/compiler/glsl/libglsl.a \
    src/mesa/glapi/shared-glapi/libglapi.a \
    src/mesa/glapi/glapi/libglapi_bridge.a \
    src/gallium/auxiliary/libgallium.a \
    src/gallium/drivers/zink/libzink.a \
    src/egl/libEGL.a \
    src/vulkan/runtime/libvulkan_runtime.a \
    src/vulkan/util/libvulkan_util.a \
    subprojects/zlib-1.3.1/libz.a