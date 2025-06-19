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
    void write(byte val, uint64_t addr) const;
    // later, we'll want to implement overloads for different width integers/datatypes
    bool search(byte val); 
    // search will search for matching bytes in _mem and stores them in _search_results.
    // If _search_results is non-empty, it will only search _search_results for
    // val. This is so the user can narrow down an address that they are looking for.

    void print_regions() const;
    void clear_results();
private:
    byte read_byte_at(mem_addr addr) ;
    bool attach_process() const;
    bool detach_process() const;
    std::map<std::string, std::vector<byte>> _mem; // key = addr_start-addr_end
    std::set<mem_addr> _search_results;
    int _pid;
};
