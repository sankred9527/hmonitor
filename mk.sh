export PKG_CONFIG_PATH=/opt/dpdk/lib64/pkgconfig/

#make static
mkdir build
meson build
ninja -C build