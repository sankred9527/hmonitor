project(
    'hmon',
    'c',
    version: '1.0.0',
    default_options: [
        'default_library=static',
        'c_std=c11',
        'cpp_std=c++11'
    ]
)

cc = meson.get_compiler('c')

dep_dpdk = dependency('libdpdk', version: '>=20.11')

dep_libconfig_dir = meson.current_source_dir() + '/lib'
message('find libconfig.a in: ' + dep_libconfig_dir)
precompiled_libconfig = cc.find_library(
    'libconfig',
    static: true,
    dirs: dep_libconfig_dir
)

subdir('src')
executable(
    'hmon',
    sources: sources,
    dependencies: [dep_dpdk, precompiled_libconfig]
)
