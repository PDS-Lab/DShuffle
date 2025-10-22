dpx_common_deps = []
dpx_common_deps += pthread_dep
dpx_common_deps += spdlog_dep
dpx_common_deps += glaze_dep
dpx_common_deps += doca_deps
dpx_common_deps += boost_fiber_dep

dpx_common_src = [
    './doca/device.cxx',
    './doca/helper.cxx',
    './doca/dpa_thread.cxx',
    './util/socket.cxx',
    './util/spin_lock.cxx',
]

dpx_common_incs = []
dpx_common_incs += basic_incs

dpx_common = library(
    'dpx_common',
    files(dpx_common_src),
    dependencies: dpx_common_deps,
    include_directories: dpx_common_incs,
    install: true,
)

dpx_common_dep = declare_dependency(
    link_with: dpx_common,
    dependencies: dpx_common_deps,
    include_directories: dpx_common_incs,
)

subdir('sd')
# subdir('spdk_spill')
subdir('trans')
subdir('native')
