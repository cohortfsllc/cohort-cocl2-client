#!/bin/sh

sdk=${HOME}/CoCl2/nacl_sdk/pepper_40
export PATH=${sdk}/toolchain/linux_pnacl/bin:${PATH}

# comment out to remove runtime gdb hooks
GDB=-DGDB=1

# one of -DSHM_TEST=1 , -DSRPC_TEST=1 , -DSTREAM_TEST=1 , -DDGRAM_TEST=1

TEST=-DDGRAM_TEST=1

echo compiling runner
g++ -g $GDB $TEST -o runner -std=c++11 -Wno-write-strings runner.cc

compile() {
    echo compiling ${2}
    ${1} $TEST -o ${2}.pexe \
	-I${sdk}/include \
	-I${sdk}/include/pnacl \
	-L${sdk}/lib/pnacl/Release \
        ${2}.cc \
	-lnacl_io
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
