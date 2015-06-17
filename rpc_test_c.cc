/*
 * Copyright 2015 CohortFS LLC, all rights reserved.
 */


#include <nacl/nacl_srpc.h>


/*
 *  The test for bool inverts the input and returns it.
 */
void BoolMethod(NaClSrpcRpc *rpc,
                NaClSrpcArg **in_args,
                NaClSrpcArg **out_args,
                NaClSrpcClosure *done) {
  out_args[0]->u.bval = !in_args[0]->u.bval;
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
}


const struct NaClSrpcHandlerDesc srpc_methods[] = {
  { "bool:b:b", BoolMethod },
#if 0
  { "double:d:d", DoubleMethod },
  { "nan::d", NaNMethod },
  { "int:i:i", IntMethod },
  { "long:l:l", LongMethod },
  { "string:s:i", StringMethod },
  { "char_array:C:C", CharArrayMethod },
  { "double_array:D:D", DoubleArrayMethod },
  { "int_array:I:I", IntArrayMethod },
  { "long_array:L:L", LongArrayMethod },
  { "null_method::", NullMethod },
  { "stringret:i:s", ReturnStringMethod },
  { "handle:h:", HandleMethod },
  { "handleret::h", ReturnHandleMethod },
  { "invalid_handle:h:", InvalidHandleMethod },
  { "invalid_handle_ret::h", ReturnInvalidHandleMethod },
#endif
  { NULL, NULL },
};


int main(int argc, char* argv[]) {
    
  if (!NaClSrpcModuleInit()) {
    return 1;
  }
  if (!NaClSrpcAcceptClientConnection(srpc_methods)) {
    return 1;
  }
  NaClSrpcModuleFini();

    return 0;
}
