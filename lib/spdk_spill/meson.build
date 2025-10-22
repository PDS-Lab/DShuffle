dpx_spdk_spill_deps = []
dpx_spdk_spill_deps += dpx_common_dep
dpx_spdk_spill_deps += spdk_dep

dpx_spdk_spill_incs = []
dpx_spdk_spill_incs += basic_incs

dpx_spdk_spill_src = ['./spdk_nvme.cxx']

dpx_spdk_spill = library(
    'dpx_spdk_spill',
    files(dpx_spdk_spill_src),
    dependencies: dpx_spdk_spill_deps,
    include_directories: dpx_spdk_spill_incs,
    install: true,
)

dpx_spdk_spill_dep = declare_dependency(
    link_with: dpx_spdk_spill,
    dependencies: dpx_spdk_spill_deps,
    include_directories: dpx_spdk_spill_incs,
)
