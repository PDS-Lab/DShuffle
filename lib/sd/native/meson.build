dpx_sd_deps = []
dpx_sd_deps += dpx_common_dep
dpx_sd_deps += simdutf_dep
dpx_sd_deps += dpa_sd_dep
dpx_sd_deps += jni_dep

dpx_sd_src = [
    './class_resolver.cxx',
    './class_walker.cxx',
    './jenv_util.cxx',
    './object_reviver.cxx',
    './object_walker.cxx',
    './sd.cxx',
    # './sd_native.cxx',
]

dpx_sd_incs = []
dpx_sd_incs += basic_incs

dpx_sd = library(
    'dpx_sd',
    files(dpx_sd_src),
    dependencies: dpx_sd_deps,
    include_directories: dpx_sd_incs,
    install: true,
)

dpx_sd_dep = declare_dependency(
    link_with: dpx_sd,
    dependencies: dpx_sd_deps,
    include_directories: dpx_sd_incs,
)
