dpx_native_deps = [dpx_common_dep, dpx_sd_dep, dpx_trans_dep, SPSCQueue_dep, MPMCQueue_dep]

dpx_native_src = [
    './serde_native.cxx',
    './sd_native.cxx',
    './trans_native.cxx',
    './naive_trans_native.cxx',
    './pipeline_trans_native.cxx',
]

dpx_native_incs = []
dpx_native_incs += basic_incs

dpx_native = library(
    'dpx_native',
    files(dpx_native_src),
    dependencies: dpx_native_deps,
    include_directories: dpx_native_incs,
    install: true,
)

dpx_native_dep = declare_dependency(
    link_with: dpx_native,
    dependencies: dpx_native_deps,
    include_directories: dpx_native_incs,
)
