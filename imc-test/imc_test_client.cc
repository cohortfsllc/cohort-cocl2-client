#include <iostream>
#include "nacl/nacl_imc_c.h"
#include <irt.h>

int main(int argc, char* argv[]) {
    std::cout << "Begin" << std::endl;

    void* f_table[100];
    size_t bytesUsed = nacl_interface_query(NACL_IRT_BASIC_v0_1,
                                            f_table,
                                            sizeof f_table);
    if (bytesUsed == 0) {
        std::cerr << "Could not load interface." << std::endl;
        exit(1);
    }

    std::cout << "Loaded table with " << bytesUsed
              << " out of " << sizeof f_table << std::endl;

    struct nacl_irt_basic* basic = (struct nacl_irt_basic*) &f_table;

    clock_t time;
    struct timeval tval;

    for (int i = 0; i < 10; ++i) {
        int result = basic->clock(&time);
        if (0 == result) std::cout << "ticks: " << time << std::endl;

        result = basic->gettod(&tval);
        if (0 == result) std::cout << "secs: " << tval.tv_sec << "."
                                   << tval.tv_usec << std::endl;

        sleep(1);
    }
    
/*
    NaClSocketAddress address = { "imc_test_client" };
    NaClHandle handle = NaClBoundSocket(&address);
    int result = NaClClose(handle);
*/
    
    std::cout << "End" << std::endl;
}
