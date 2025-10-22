naive_offload_server_src = ['naive_offload_server.cxx']
naive_offload_server_dep = [dpx_native_dep, jni_dep]

cmd = run_command('sh', '-c', 'echo $JAVA_HOME', check: true)
java_home = cmd.stdout().strip()
message('JAVA_HOME=' + java_home)
if build_machine.cpu_family() == 'aarch64'
    libjvm_path = (java_home + '/jre/lib/aarch64/server')
elif build_machine.cpu_family() == 'x86_64'
    libjvm_path = (java_home + '/jre/lib/amd64/server')
else
    error('unsupported arch')
endif

executable(
    'naive_offload_server',
    files(naive_offload_server_src),
    dependencies: naive_offload_server_dep,
    build_rpath: libjvm_path,
    install_rpath: libjvm_path,
)

jar('Partitioner', 'Partitioner.java')
