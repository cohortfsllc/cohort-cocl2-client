#ifndef RUNNER_H
#define RUNNER_H

extern int calculateOsds(const std::string& algorithm_name,
                         const uuid_t& uuid,
                         const char* object_name,
                         const uint32_t osds_requested,
                         uint32_t osd_list[]);

#endif // RUNNER_H
