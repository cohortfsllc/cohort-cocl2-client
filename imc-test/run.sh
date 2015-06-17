#!/bin/bash

#
# Copyright 2015 CohortFS LLC, all rights reserved.
#


sel_ldr="${HOME}/CoCl2/native_client/native_client/scons-out/dbg-linux-x86-64/staging/sel_ldr"
irt="${HOME}/CoCl2/native_client/native_client/scons-out/nacl_irt-x86-64/staging/irt_core.nexe"
nexe="imc_test_client_x86_64.nexe"


cat >nacl-gdb.cmds <<EOF
target remote localhost:4014
nacl-irt `readlink -f $irt`
file `readlink -f $nexe`
EOF

if [[ $0 == *"debug.sh"* ]] ;then
    cat >runner-gdb.cmds <<EOF
set args -s $sel_ldr -i $irt -n $nexe $@
EOF
    set -x
    gdb runner -x runner-gdb.cmds
else
    set -x
    ./runner -s $sel_ldr -i $irt -n $nexe "$@"
fi
