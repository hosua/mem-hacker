# mem-hacker

### Features that I'd love to add

- [ ] add setting to search values by hex or decimal (even better, automatically treat as hex when prefixed with 0x)
- [X] add setting to switch between searching/writing data types
- [ ] multi-threading support (big dick points)
- [ ] GUI (big dick points)

### Methods to implement

- [ ] `void MemoryTool::write(int8_t val, uint64_t addr) const;`
- [ ] `void MemoryTool::write(int16_t val, uint64_t addr) const;`
- [ ] `void MemoryTool::write(int32_t val, uint64_t addr) const;`
- [ ] `void MemoryTool::write(int64_t val, uint64_t addr) const;`
- [ ] `void MemoryTool::write(float val, uint64_t addr) const;`
- [ ] `void MemoryTool::write(double val, uint64_t addr) const;`
- [X] `bool MemoryTool::search(uint32_t val);`
- [ ] `bool MemoryTool::search(uint64_t val);`
- [ ] `bool MemoryTool::search(int8_t val);`
- [ ] `bool MemoryTool::search(int16_t val);`
- [ ] `bool MemoryTool::search(int32_t val);`
- [ ] `bool MemoryTool::search(int64_t val);`
- [X] `uint32_t MemoryTool::read_uint32_at(mem_addr addr) const;`
- [ ] `uint64_t MemoryTool::read_uint64_at(mem_addr addr) const;`
- [ ] `int8_t   MemoryTool::read_int8_at(mem_addr addr) const;`
- [ ] `int16_t  MemoryTool::read_int16_at(mem_addr addr) const;`
- [ ] `int32_t  MemoryTool::read_int32_at(mem_addr addr) const;`
- [ ] `int64_t  MemoryTool::read_int64_at(mem_addr addr) const;`

