#!/bin/bash

# usage package_ps5_libs.sh builddir packagename

BuildDir=$1
PackageName=$2

rm -rf $BuildDir/$PackageName

mkdir -p $BuildDir/$PackageName/include
mkdir -p $BuildDir/$PackageName/lib

cd $BuildDir
for lib in `find -name '*.a'`;
    do ar -t $lib | xargs ar rvs $lib.new && mv -v $lib.new $lib;
done
cd ../

# Copy libs
cp $BuildDir/src/util/blake3/libblake3.a $BuildDir/$PackageName/lib/
cp $BuildDir/src/c11/impl/libmesa_util_c11.a $BuildDir/$PackageName/lib/
cp $BuildDir/src/util/libmesa_util_simd.a $BuildDir/$PackageName/lib/
cp $BuildDir/src/util/libmesa_util.a $BuildDir/$PackageName/lib/
cp $BuildDir/src/compiler/libcompiler.a $BuildDir/$PackageName/lib/
cp $BuildDir/src/compiler/nir/libnir.a $BuildDir/$PackageName/lib/
cp $BuildDir/src/compiler/spirv/libvtn.a $BuildDir/$PackageName/lib/
cp $BuildDir/src/compiler/glsl/libglsl_util.a $BuildDir/$PackageName/lib/
cp $BuildDir/src/compiler/glsl/glcpp/libglcpp.a $BuildDir/$PackageName/lib/
cp $BuildDir/src/compiler/glsl/libglsl.a $BuildDir/$PackageName/lib/
cp $BuildDir/src/mesa/libmesa.a $BuildDir/$PackageName/lib/
cp $BuildDir/src/mesa/glapi/shared-glapi/libglapi.a $BuildDir/$PackageName/lib/
cp $BuildDir/src/mesa/glapi/glapi/libglapi_bridge.a $BuildDir/$PackageName/lib/
cp $BuildDir/src/gallium/auxiliary/libgallium.a $BuildDir/$PackageName/lib/
cp $BuildDir/src/gallium/drivers/softpipe/libsoftpipe.a $BuildDir/$PackageName/lib/
cp $BuildDir/src/util/libmesa_util_clflush.a $BuildDir/$PackageName/lib/
cp $BuildDir/src/util/libmesa_util_clflushopt.a $BuildDir/$PackageName/lib/
cp $BuildDir/src/mesa/libmesa_sse41.a $BuildDir/$PackageName/lib

# Copy includes
cp -r include/EGL $BuildDir/$PackageName/include/EGL
cp -r include/GL $BuildDir/$PackageName/include/GL
cp -r include/KHR $BuildDir/$PackageName/include/KHR

