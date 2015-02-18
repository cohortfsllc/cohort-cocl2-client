#!/bin/sh

sdk=${HOME}/CoCl2/nacl_sdk/pepper_40
export PATH=${sdk}/toolchain/linux_pnacl/bin:${PATH}

TEST=-DDGRAM_TEST=1
# TEST=-DSHM_TEST=0 -DSRPC_TEST=0 -DSTREAM_TEST=0 -DDGRAM_TEST=0

echo compiling runner
g++ -g $TEST -o runner -std=c++11 -Wno-write-strings runner.cc

compile() {
    echo compiling ${2}
    ${1} $TEST -o ${2}.pexe \
	-I${sdk}/include/pnacl \
        ${2}.cc
    echo translating ${2}
    pnacl-translate -arch x86-64 --allow-llvm-bitcode-input \
        -o ${2}_x86_64.nexe \
        ${2}.pexe
    echo $?
    echo "Built ${2}_x86_64.nexe."
}

compile_cpp() {
    compile pnacl-clang++ ${1}
}

compile_c() {
    compile pnacl-clang ${1}
}


compile_cpp message_test_client

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
