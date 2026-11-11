#!/bin/bash
rm -rf build_clc

meson setup build_clc \
    -Dbuildtype=release \
    -Dmesa-clc=enabled \
    -Dinstall-mesa-clc=true \
    -Dgallium-drivers= \
    -Dvulkan-drivers= \
    -Dplatforms= \
    -Dglx=disabled \
    -Degl=disabled \
    -Dgbm=disabled \
    -Dopengl=false \
    -Dgles1=disabled \
    -Dgles2=disabled \
    -Dllvm=enabled \
    -Dshared-llvm=enabled \
    -Dbuild-tests=false \
    -Dvalgrind=disabled \
    -Dlibunwind=disabled \
    -Dzstd=disabled \
    -Dxmlconfig=disabled \
    -Dtools= 

ninja -C build_clc src/compiler/clc/mesa_clc src/compiler/spirv/vtn_bindgen2

mkdir -p clc_bin
cp "build_clc/src/compiler/clc/mesa_clc" "clc_bin/mesa_clc"
cp "build_clc/src/compiler/spirv/vtn_bindgen2" "clc_bin/vtn_bindgen2"