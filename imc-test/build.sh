#!/bin/sh

#
# Copyright 2015 CohortFS LLC, all rights reserved.
#


sdk="${HOME}/CoCl2/nacl_sdk/pepper_current"
export PATH=${sdk}/toolchain/linux_pnacl/bin:${PATH}

echo compiling runner
g++ -g -o runner -pthread -std=c++11 -Wno-write-strings \
    runner.cc messaging.cc alg_info.cc call_return.cc \
    epoch_gen.cc debug.c testing.cc \
    -luuid

exit_on_error() {
    if [ $? -ne 0 ] ;then
	echo "Exiting due to early error."
	exit 1
    fi
}

compile() {
    echo compiling and linking ${2}
    ${1} -g -o ${2}.pexe ${2}.cc -luuid

    exit_on_error

    echo translating ${2}
    pnacl-translate -arch x86-64 --allow-llvm-bitcode-input \
        -o ${2}_x86_64.nexe \
        ${2}.pexe

    exit_on_error

    echo "Built ${2}_x86_64.nexe."
}

compile_cpp() {
    compile pnacl-clang++ ${1}
}

compile_c() {
    compile pnacl-clang ${1}
}


compile_cpp imc_test_client

exit 0

compile_cpp rpc_test_c

pnacl-clang++ -I../nacl -lsrpc -o .pexe rpc_test_c.cc 
pnacl-translate -arch x86-64 --allow-llvm-bitcode-input \
    rpc_test_c.pexe \
    -o rpc_test_c_x86_64.nexe

pnacl-clang hello.c -o hello.pexe
pnacl-translate -arch x86-64 --allow-llvm-bitcode-input \
    hello.pexe \
    -o hello_x86_64.nexe
