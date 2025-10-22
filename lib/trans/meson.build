dpx_trans_deps = []
dpx_trans_deps += dpx_common_dep
dpx_trans_deps += zpp_bits_dep
dpx_trans_deps += uring_dep

dpx_trans_incs = []
dpx_trans_incs += basic_incs

dpx_trans_src = [
    './priv/tcp/connection.cxx',
    './priv/tcp/endpoint.cxx',
    './priv/doca/comch/connection.cxx',
    './priv/doca/comch/endpoint.cxx',
    './priv/doca/rdma/connection.cxx',
    './priv/doca/rdma/endpoint.cxx',
]

dpx_trans = library(
    'dpx_trans',
    files(dpx_trans_src),
    dependencies: dpx_trans_deps,
    include_directories: dpx_trans_incs,
    install: true,
)

dpx_trans_dep = declare_dependency(
    link_with: dpx_trans,
    dependencies: dpx_trans_deps,
    include_directories: dpx_trans_incs,
)
