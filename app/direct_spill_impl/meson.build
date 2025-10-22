host_daemon_src = ['host_daemon.cxx']
host_daemon_dep = [dpx_trans_dep]

executable(
    'host_daemon',
    files(host_daemon_src),
    dependencies: host_daemon_dep,
)

dpu_file_spill_src = ['dpu_file_spill.cxx']
dpu_file_spill_dep = [dpx_trans_dep]

executable(
    'dpu_file_spill',
    files(dpu_file_spill_src),
    dependencies: dpu_file_spill_dep,
)
