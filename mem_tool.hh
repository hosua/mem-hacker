#pragma once

#include <stdint.h>
#include <map>
#include <set>
#include <string>
#include <vector>

typedef uint64_t mem_addr;

typedef enum {
    DTM_U8,
    DTM_U16,
    DTM_U32,
    DTM_U64,
    DTM_I8,
    DTM_I16,
    DTM_I32,
    DTM_I64,
    DTM_FLOAT,
    DTM_DOUBLE,
} DatatypeMode;

extern std::vector<std::pair<DatatypeMode, std::string>> datatypeModeStringMap;

class MemoryTool {
public:
    MemoryTool();
    ~MemoryTool();
    
    int dump(int pid); // dumps memory from pid into _mem
    void write(uint8_t val, uint64_t addr) const;
    void write(uint16_t val, uint64_t addr) const;
    void write(uint32_t val, uint64_t addr) const;
    void write(uint64_t val, uint64_t addr) const;
    void write(int8_t val, uint64_t addr) const;  // TODO
    void write(int16_t val, uint64_t addr) const; // TODO
    void write(int32_t val, uint64_t addr) const; // TODO
    void write(int64_t val, uint64_t addr) const; // TODO
    void write(float val, uint64_t addr) const;   // TODO
    void write(double val, uint64_t addr) const;  // TODO
    
    // searches existing results if some are stored
    bool search(uint8_t val); 
    bool search(uint16_t val); 
    bool search(uint32_t val); // TODO
    bool search(uint64_t val); // TODO
    bool search(int8_t val);   // TODO
    bool search(int16_t val);  // TODO
    bool search(int32_t val);  // TODO
    bool search(int64_t val);  // TODO

    void print_regions() const;
    void clear_results();

    void set_datatype_mode(DatatypeMode mode);
    DatatypeMode get_datatype_mode() const;
    
    std::vector<mem_addr> list_search_results() const;
private:
    uint8_t read_uint8_at(mem_addr addr) const;
    uint16_t read_uint16_at(mem_addr addr) const;
    uint32_t read_uint32_at(mem_addr addr) const; // TODO
    uint64_t read_uint64_at(mem_addr addr) const; // TODO
    int8_t read_int8_at(mem_addr addr) const;     // TODO
    int16_t read_int16_at(mem_addr addr) const;   // TODO
    int32_t read_int32_at(mem_addr addr) const;   // TODO
    int64_t read_int64_at(mem_addr addr) const;   // TODO

    bool attach_process() const;
    bool detach_process() const;
    
    DatatypeMode _datatype_mode = DTM_U32;
    std::map<std::string, std::vector<uint8_t>> _mem; // key = addr_start-addr_end
    std::set<mem_addr> _search_results;
    int _pid;
};
