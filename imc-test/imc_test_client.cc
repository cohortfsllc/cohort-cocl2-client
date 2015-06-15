#include <stdlib.h>
#include <unistd.h>
#include <iostream>
#include <sys/time.h>
#include <uuid/uuid.h>

#include <irt.h>
#include <irt_dev.h>
#include "irt_cocl2.h"


const int stack_size_hint = 128 * 1024; // 128K


int shutdown(void) {
    return 0;
}


int alloc_shared_mem(unsigned size,
                     int* mem_handle) {
    return 0;
}

int free_shared_mem(int handle) {
    return 0;
}

int compute_osds(const uuid_opaque vol_uuid,
                 const char* obj_name,
                 uint32_t* osd_list,
                 const int osd_count) {
#if 0
    {
        std::ios::fmtflags f( std::cout.flags() );
        std::cout << "vol_uuid " << (void*) vol_uuid << std::endl;
        std::cout << "obj_name " << (void*) obj_name << std::endl;
        std::cout << std::hex;
        for (int i = 0; i < 16; ++i) { std::cout << (int) vol_uuid[i] << " "; }
        std::cout << std::endl;
        std::cout.flags(f); 
    }
#endif
    uuid_t* uuid = (uuid_t*) &vol_uuid;

#if 0
    char uuid_text[256];
    uuid_unparse(*uuid, uuid_text);
    
    std::cout << "inner compute_osds got called with object named " <<
        obj_name << ", volume uuid of " << uuid_text <<
        " is asking for " << osd_count << " osds." <<
        std::endl;
#endif
    for (int i = 0; i < osd_count; ++i) {
        osd_list[i] = -(osd_count - i);
    }

    return 0;
}

struct cocl2_interface my_cocl2_interface = {
    shutdown,
    alloc_shared_mem,
    free_shared_mem,
    compute_osds,
};


// NULL error, otherwise valid
template<typename T>
const T* query_interface(const char* name) {
    T* table = new T();

    if (!table) {
        std::cerr << "Could not load " << name << " interface." << std::endl;
        return NULL;
    }

    size_t bytesUsed = nacl_interface_query(name, table, sizeof(T));
                                            
    if (bytesUsed == 0) {
        std::cerr << "Could not load " << name << " interface." << std::endl;
        return NULL;
    }

    return table;
}


void check_result(const char* name, int result) {
    if (result) {
        std::cerr << "Error with " << name << ", " <<
            result << "." << std::endl;
        exit(1);
    } else {
        std::cerr << "Success with " << name << 
            "." << std::endl;
    }
}


// 0 success, non-zero error
int test_proof_of_concept(const struct nacl_irt_cocl2* cocl2) {
    const int i1 = 5;
    const int i2 = 8;
    int result;
    cocl2->cocl2_test(i1, i2, &result);

    std::cout << "The proof of concept result is " << result <<
        " and should be " <<
        (i1 + i2) << "." << std::endl;

    return 0;
}


// 0 success, non-zero error
int test_my_gettod(const struct nacl_irt_cocl2* cocl2) {
    int result;
    struct timeval tval;

    result = cocl2->cocl2_gettod(&tval);
    std::cout << "The result cocl2_gettod is " << result <<
        " and the pid is " << tval.tv_sec << "." << std::endl;

    sleep(1);

    result = cocl2->cocl2_gettod(&tval);
    std::cout << "The result cocl2_gettod is " << result <<
        " and the pid is " << tval.tv_sec << "." << std::endl;

    return 0;
}


// 0 success, non-zero error
int test_time(const struct nacl_irt_basic* basic, int count) {
    int result;
    clock_t time;
    struct timeval tval;

    for (int i = 0; i < count; ++i) {
        if (i > 0) {
            sleep(1);
        }

        // result = basic->clock(&time);
        // if (0 == result) std::cout << "ticks: " << time << std::endl;

        result = basic->gettod(&tval);
        if (0 == result) std::cout << "secs: " << tval.tv_sec << "."
                                   << tval.tv_usec << std::endl;

    }

    return 0;
}



int test_cocl2_init(const struct nacl_irt_cocl2* cocl2,
                    const int bootstrap_socket_addr) {
    int result = cocl2->cocl2_init(bootstrap_socket_addr,
                                   "imc_test_client",
                                   stack_size_hint,
                                   &my_cocl2_interface);
    std::cout << "The result of cocl2_init is " << result << std::endl;
    return result;
}


int main(int argc, char* argv[]) {
    std::cout << "Begin" << std::endl;

    int bootstrap_socket_addr = -1;

    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " bootstrap_socket_addr" <<
            std::endl;
        goto end;
    }

    bootstrap_socket_addr = atoi(argv[1]);
    std::cout << "Bootstrap socket addr is " <<
        bootstrap_socket_addr << std::endl;

    {
#if 0
        const struct nacl_irt_basic* basic =
            query_interface<struct nacl_irt_basic>(NACL_IRT_BASIC_v0_1);

        check_result("time", test_time(basic, 1));
#endif

        const struct nacl_irt_cocl2* cocl2 =
            query_interface<struct nacl_irt_cocl2>(NACL_IRT_COCL2_v0_1);

        check_result("proof-of-concept", test_proof_of_concept(cocl2));
        // check_result("my_gettod", test_my_gettod(cocl2));
        check_result("my_cocl2_init",
                     test_cocl2_init(cocl2, bootstrap_socket_addr));
      
        delete cocl2;
        // delete basic;
    }

end:
    std::cout << "End" << std::endl;
}
