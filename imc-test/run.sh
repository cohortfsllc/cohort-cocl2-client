#!/bin/sh

sel_ldr="${HOME}/CoCl2/native_client/native_client/scons-out/dbg-linux-x86-64/staging/sel_ldr"
irt="${HOME}/CoCl2/native_client/native_client/scons-out/nacl_irt-x86-64/staging/irt_core.nexe"
nexe="imc_test_client_x86_64.nexe"

set -x
${sel_ldr} -B ${irt} -- ${nexe}
