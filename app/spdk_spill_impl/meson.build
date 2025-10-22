host_file_server_src = ['host_file_server.cxx']
host_file_server_dep = [dpx_spdk_spill_dep, dpx_trans_dep]

executable(
    'host_file_server',
    files(host_file_server_src),
    dependencies: host_file_server_dep,
)

dpu_file_spill_src = ['dpu_file_spill.cxx']
dpu_file_spill_dep = [dpx_spdk_spill_dep, dpx_trans_dep]

executable(
    'dpu_file_spill',
    files(dpu_file_spill_src),
    dependencies: dpu_file_spill_dep,
)
