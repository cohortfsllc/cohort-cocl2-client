#!/bin/sh

cd ${HOME}/CoCl2/nacl2/native_client
./scons MODE=dbg-linux,nacl platform=x86-64
