#pragma once

#include <stdint.h>
#include <map>
#include <set>
#include <string>
#include <vector>

typedef uint64_t mem_addr;
typedef uint64_t word; 
typedef uint8_t byte;

class MemoryTool {
public:
    MemoryTool();
    ~MemoryTool();
    
    int read(int pid); // dumps memory from pid into _mem
    void write(uint8_t val, uint64_t addr) const;
    void write(uint16_t val, uint64_t addr) const;
    void write(uint32_t val, uint64_t addr) const;
    void write(uint64_t val, uint64_t addr) const;
    // later, we'll want to implement overloads for different width integers/datatypes
    bool search(byte val); // searches existing results if some are stored

    void print_regions() const;
    void clear_results();
    
    std::vector<mem_addr> list_search_results() const;
private:
    byte read_byte_at(mem_addr addr) ;
    bool attach_process() const;
    bool detach_process() const;
    std::map<std::string, std::vector<byte>> _mem; // key = addr_start-addr_end
    std::set<mem_addr> _search_results;
    int _pid;
};
