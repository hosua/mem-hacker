#pragma once

#include <stdint.h>
#include <map>
#include <set>
#include <string>
#include <vector>

typedef uint64_t mem_addr;

// unsigned is redundant, we can cast all int types and search/write
// accordingly since the bit structure is all that matters.
typedef enum {
    // DTM_U8,
    // DTM_U16,
    // DTM_U32,
    // DTM_U64,
    DTM_I8,
    DTM_I16,
    DTM_I32,
    DTM_I64,
    DTM_FLOAT,
    DTM_DOUBLE,
} DatatypeMode;

extern std::vector<std::pair<DatatypeMode, std::string>> datatype_mode_string_map;

class MemoryTool {
public:
    MemoryTool();
    ~MemoryTool();
    
    int dump(int pid); // dumps memory from pid into _mem
    
    void write(uint8_t val, mem_addr addr) const;
    void write(uint16_t val, mem_addr addr) const;
    void write(uint32_t val, mem_addr addr) const;
    void write(uint64_t val, mem_addr addr) const;
    void write(float val, mem_addr addr) const;   
    void write(double val, mem_addr addr) const;  // TODO
    
    // searching memory with ptrace if no results exist in _mem. searches
    // existing results in _mem otherwise. 
    bool search(uint8_t val); 
    bool search(uint16_t val); 
    bool search(uint32_t val); 
    bool search(uint64_t val); 
    bool search(float val); // TODO: Approximate float search
    bool search(double val); // TODO

    void print_regions() const;
    void clear_results();

    void set_datatype_mode(DatatypeMode mode);
    DatatypeMode get_datatype_mode() const;
    
    std::vector<mem_addr> list_search_results() const; // old ui 
    std::vector<std::string> get_search_list() const; // FTXUI

private:
    uint8_t read_uint8_at(mem_addr addr) const;
    uint16_t read_uint16_at(mem_addr addr) const;
    uint32_t read_uint32_at(mem_addr addr) const; 
    uint64_t read_uint64_at(mem_addr addr) const; 

    bool attach_process() const;
    bool detach_process() const;
    
    DatatypeMode _datatype_mode = DTM_I32;
    std::map<std::string, std::vector<uint8_t>> _mem; // key = "addr_start-addr_end"
    std::set<mem_addr> _search_results;
    int _pid;
};
