#include "mem_tool.hh"

#include <cstring>
#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <unistd.h>
#include <format>
#include <fstream>
#include <iostream>

#include "logger.hh"

static const size_t WORD_SIZE = sizeof(long);

std::vector<std::pair<DatatypeMode, std::string>> datatype_mode_string_map = {
    // {DTM_U8,     "uint8_t" },
    // {DTM_U16,    "uint16_t"},
    // {DTM_U32,    "uint32_t"},
    // {DTM_U64,    "uint64_t"},
    {DTM_I8,     "int8_t"  },
    {DTM_I16,    "int16_t" },
    {DTM_I32,    "int32_t" },
    {DTM_I64,    "int64_t" },
    {DTM_FLOAT,  "float"   },
    {DTM_DOUBLE, "double"  },
};

static std::vector<std::string> split_string(const std::string& str, const std::string& delim) {
    std::vector<std::string> res;
    std::string s = str;
    size_t pos = 0;
    while ( (pos = s.find(delim)) != std::string::npos) {
        std::string tok = s.substr(0, s.find(delim));
        res.emplace_back(s.substr(0, s.find(delim)));
        s.erase(0, pos + delim.length());
    }
    res.emplace_back(s);
    return res;
}

MemoryTool::MemoryTool() {}
MemoryTool::~MemoryTool() {}

int MemoryTool::dump(int pid) {
    _pid = pid;
    std::string mem_file = "/proc/" + std::to_string(_pid) + "/mem";
    int fd = open(mem_file.c_str(), O_RDONLY);
    if (fd == -1) {
        perror("open mem_file");
        logger << strerror(errno);
        return -1;
    }
    
    std::string mem_map_file = "/proc/" + std::to_string(_pid) + "/maps";
    std::ifstream file(mem_map_file, std::ios::in);
    if (file.is_open()) {
        std::string line;
        while (std::getline(file, line)) {
            if (line.find("rw-p") != std::string::npos &&     // get readable/writable memory only
                line.find("/usr/lib") == std::string::npos && // ignore libraries
                line.find("[stack]") == std::string::npos     // ignore the stack (it's too volatile to be worthwhile)
            ) {
                // parse start and end address;
                std::vector<std::string> start_end = split_string(split_string(line, " ")[0], "-");
                const mem_addr addr_start = std::stoull(start_end[0], nullptr, 16), 
                               addr_end = std::stoull(start_end[1], nullptr, 16);
                _mem[std::string(std::format("{:x}", addr_start) + "-" + std::format("{:x}", addr_end))] = {};
                // logger << addr_start <<  " to "  << addr_end << "\n";
            }
        }
    } else {
        perror("ifstream");
        logger << strerror(errno);
        return -1;
    }
    
    // Not sure why this doesn't work when I try to pread() in the first loop.
    for (auto& [region, memory]: _mem) {
        std::vector<std::string> start_end = split_string(region, "-");
        const mem_addr addr_start = std::stoull(start_end[0], nullptr, 16),
                       addr_end = std::stoull(start_end[1], nullptr, 16);

        size_t n_bytes = addr_end - addr_start; 
        memory = std::vector<uint8_t>(n_bytes, 0);
        pread(fd, (void*)memory.data(), n_bytes, addr_start);
    }

    return 0;
}

/* READ METHODS */

uint8_t MemoryTool::read_uint8_at(mem_addr addr) const {
    mem_addr aligned_addr = addr & ~(WORD_SIZE - 1);
    uint64_t data = ptrace(PTRACE_PEEKDATA, _pid, (void*)aligned_addr, nullptr);
    if (data == -1) {
        perror("ptrace PEEKDATA");
        logger << strerror(errno);
        return -1;
    }
    const size_t byte_offset = addr % WORD_SIZE;
    uint8_t byte = (data >> (8 * byte_offset)) & 0xFF;
    return byte;
}

uint16_t MemoryTool::read_uint16_at(mem_addr addr) const {
    const size_t byte_offset = addr % WORD_SIZE;
    const mem_addr aligned_addr = addr & ~(WORD_SIZE - 1);

    errno = 0;
    uint64_t lo = ptrace(PTRACE_PEEKDATA, _pid, (void*)aligned_addr, nullptr);
    if (lo == -1ULL && errno != 0) {
        perror("ptrace PEEKDATA");
        logger << strerror(errno);
        return 0;
    }

    if (byte_offset <= WORD_SIZE - sizeof(uint16_t)) {
        return (lo >> (8 * byte_offset)) & 0xFFFF;
    }

    // Crosses boundary — need next word
    errno = 0;
    uint64_t hi = ptrace(PTRACE_PEEKDATA, _pid, (void*)(aligned_addr + WORD_SIZE), nullptr);
    if (hi == -1ULL && errno != 0) {
        perror("ptrace PEEKDATA (next word)");
        logger << strerror(errno);
        return 0;
    }

    uint8_t b0 = (lo >> (8 * byte_offset)) & 0xFF;
    uint8_t b1 = hi & 0xFF;
    return static_cast<uint16_t>(b0 | (b1 << 8));
}

uint32_t MemoryTool::read_uint32_at(mem_addr addr) const {
    const size_t byte_offset = addr % WORD_SIZE;
    const mem_addr aligned_addr = addr & ~(WORD_SIZE - 1);
    errno = 0;
    uint64_t lo = ptrace(PTRACE_PEEKDATA, _pid, (void*)aligned_addr, nullptr);
    if (lo == -1ULL && errno != 0) {
        perror("ptrace PEEKDATA");
        logger << strerror(errno);
        return 0;
    }

    if (byte_offset <= WORD_SIZE - sizeof(uint32_t)) {
        return (lo >> (8 * byte_offset)) & 0xFFFFFFFF;
    }

    errno = 0;
    uint64_t hi = ptrace(PTRACE_PEEKDATA, _pid, (void*)(aligned_addr + WORD_SIZE), nullptr);
    if (hi == -1ULL && errno != 0) {
        perror("ptrace PEEKDATA (next word)");
        logger << strerror(errno);
        return 0;
    }

    uint32_t res = 0;
    for (int i = 0; i < 4; ++i) {
        uint8_t byte = 
            (byte_offset + i) < WORD_SIZE ?
            (lo >> (8 * (byte_offset + i))) & 0xFF 
            :
            (hi >> (8 * (byte_offset + i - WORD_SIZE))) & 0xFF;

        res |= byte << (8 * i);
    }
    return res;
}

uint64_t MemoryTool::read_uint64_at(mem_addr addr) const {
    const size_t byte_offset = addr % WORD_SIZE;
    const mem_addr aligned_addr = addr & ~(WORD_SIZE - 1);
    errno = 0;
    uint64_t lo = ptrace(PTRACE_PEEKDATA, _pid, (void*)aligned_addr, nullptr);
    if (lo == -1ULL && errno != 0) {
        perror("ptrace PEEKDATA");
        logger << strerror(errno);
        return 0;
    }

    if (byte_offset <= WORD_SIZE - sizeof(uint64_t)) {
        return (lo >> (8 * byte_offset));
    }

    errno = 0;
    uint64_t hi = ptrace(PTRACE_PEEKDATA, _pid, (void*)(aligned_addr + WORD_SIZE), nullptr);
    if (hi == -1ULL && errno != 0) {
        perror("ptrace PEEKDATA (next word)");
        logger << strerror(errno);
        return 0;
    }

    uint64_t res = 0;
    for (int i = 0; i < 8; ++i) {
        uint8_t byte = 
            (byte_offset + i) < WORD_SIZE ?
            (lo >> (8 * (byte_offset + i))) & 0xFF 
            :
            (hi >> (8 * (byte_offset + i - WORD_SIZE))) & 0xFF;

        res |= static_cast<uint64_t>(byte) << (8 * i);
    }
    return res;
}

/* SEARCH METHODS */

bool MemoryTool::search(uint8_t val) {
    logger << "searching uint8_t...\n";
    attach_process();
    // clean search
    if (_search_results.size() == 0) {
        for (const auto& [region, bytes] : _mem) {
            std::vector<std::string> start_end = split_string(region, "-");
            const mem_addr addr_start = std::stoull(start_end[0], nullptr, 16),
                           addr_end = std::stoull(start_end[1], nullptr, 16);
            
            int offset = 0;
            for (const uint8_t& b : bytes) {
                const mem_addr addr = addr_start + offset;
                if (val == b) _search_results.insert(addr);
                offset++;
            }
        }
        logger << std::format("Found {:d} results.\n", _search_results.size());
    } else {
        std::erase_if(_search_results, [&](const mem_addr& addr) {
            uint8_t byte = read_uint8_at(addr);
            if (byte == val) logger << std::format("0x{:x}: {:d}\n", addr, byte);
            return byte != val;
        });
        logger << std::format("{:d} results remain.\n", _search_results.size());
    }
    detach_process();
    return true;
}

bool MemoryTool::search(uint16_t val) {
    logger << "searching uint16_t...\n";
    attach_process();
    // clean search
    if (_search_results.size() == 0) {
        for (const auto& [region, bytes] : _mem) {
            std::vector<std::string> start_end = split_string(region, "-");
            const mem_addr addr_start = std::stoull(start_end[0], nullptr, 16),
                           addr_end = std::stoull(start_end[1], nullptr, 16);
            
            for (int offset = 0; offset < bytes.size()-1; ++offset) {
                const mem_addr addr = addr_start + offset;
                const uint8_t b1 = bytes[offset], b2 = bytes[offset+1];
                const uint16_t other_val = b1 | (b2 << 8);
                if (val == other_val){ 
                    _search_results.insert(addr);
                }
            }
        }
        logger << std::format("Found {:d} results.\n", _search_results.size());
    } else {
        std::erase_if(_search_results, [&](const mem_addr& addr) {
            uint16_t data = read_uint16_at(addr);
            return data != val;
        });
        logger << std::format("{:d} results remain.\n", _search_results.size());
    }
    detach_process();
    return true;
}

bool MemoryTool::search(uint32_t val) {
    logger << "searching uint32_t...\n";
    attach_process();
    // clean search
    if (_search_results.size() == 0) {
        for (const auto& [region, bytes] : _mem) {
            std::vector<std::string> start_end = split_string(region, "-");
            const mem_addr addr_start = std::stoull(start_end[0], nullptr, 16),
                           addr_end = std::stoull(start_end[1], nullptr, 16);
            
            for (int offset = 0; offset < bytes.size()-3; ++offset) {
                const mem_addr addr = addr_start + offset;
                const uint32_t other_val = 
                              bytes[offset] | 
                    (bytes[offset+1] <<  8) | 
                    (bytes[offset+2] << 16) | 
                    (bytes[offset+3] << 24);
                if (val == other_val) { 
                    _search_results.insert(addr);
                }
            }
        }
        logger << std::format("Found, {:d}, results.\n", _search_results.size());
    } else {
        std::erase_if(_search_results, [&](const mem_addr& addr) {
            uint32_t data = read_uint32_at(addr);
            return data != val;
        });
        logger << std::format("{:d} results remain.\n", _search_results.size());
    }
    detach_process();
    return true;
}

bool MemoryTool::search(uint64_t val) {
    logger << "searching uint64_t...\n";
    attach_process();
    // clean search
    if (_search_results.size() == 0) {
        for (const auto& [region, bytes] : _mem) {
            std::vector<std::string> start_end = split_string(region, "-");
            const mem_addr addr_start = std::stoull(start_end[0], nullptr, 16),
                           addr_end = std::stoull(start_end[1], nullptr, 16);
            for (int offset = 0; offset < bytes.size()-7; ++offset) {
                const mem_addr addr = addr_start + offset;
                const uint64_t other_val = 
                              bytes[offset] | 
                    (bytes[offset+1] <<  8) | 
                    (bytes[offset+2] << 16) | 
                    (bytes[offset+3] << 24) |
                    (static_cast<uint64_t>(bytes[offset+4]) << 32) |
                    (static_cast<uint64_t>(bytes[offset+5]) << 40) |
                    (static_cast<uint64_t>(bytes[offset+6]) << 48) |
                    (static_cast<uint64_t>(bytes[offset+7]) << 56);
                if (val == other_val){ 
                    _search_results.insert(addr);
                }
            }
        }
        logger << std::format("Found, {:d}, results.\n", _search_results.size());
    } else {
        std::erase_if(_search_results, [&](const mem_addr& addr) {
            uint64_t data = read_uint64_at(addr);
            return data != val;
        });
        logger << std::format("{:d} results remain.\n", _search_results.size());
    }
    detach_process();
    return true;
}

// TODO: We need to approximate how we search for floats for this to be useful 
bool MemoryTool::search(float val) {
    logger << "searching float...\n";
    attach_process();
    if (_search_results.size() == 0) {
        for (const auto& [region, bytes] : _mem) {
            std::vector<std::string> start_end = split_string(region, "-");
            const mem_addr addr_start = std::stoull(start_end[0], nullptr, 16),
                           addr_end = std::stoull(start_end[1], nullptr, 16);
            
            for (int offset = 0; offset < bytes.size()-10; ++offset) {
                const mem_addr addr = addr_start + offset;
                const uint32_t other_val = 
                        static_cast<uint32_t>(bytes[offset]) |
                        (static_cast<uint32_t>(bytes[offset + 1]) << 8) |
                        (static_cast<uint32_t>(bytes[offset + 2]) << 16) |
                        (static_cast<uint32_t>(bytes[offset + 3]) << 24);
                if (std::bit_cast<float>(other_val) == val){ 
                    _search_results.insert(addr);
                }
            }
        }
        logger << std::format("Found, {:d}, results.\n", _search_results.size());
    } else {
        std::erase_if(_search_results, [&](const mem_addr& addr) {
            float data = std::bit_cast<float>(read_uint32_at(addr));
            return data != val;
        });
        logger << std::format("{:d} results remain.\n", _search_results.size());
    }
    detach_process();
    return true;
}

/* WRITE METHODS */

void MemoryTool::write(uint8_t val, mem_addr addr) const {
    logger << std::format("writing uint8 to 0x{:X}...\n", addr);
    attach_process();
    mem_addr aligned_addr = addr & ~(WORD_SIZE - 1);
    size_t byte_offset = addr % WORD_SIZE;
    errno = 0;
    long word = ptrace(PTRACE_PEEKDATA, _pid, (void*)aligned_addr, nullptr);
    if (word == -1 && errno != 0) {
        perror("ptrace PEEKDATA");
        logger << strerror(errno);
        exit(EXIT_FAILURE);
    }
    
    // TODO: My current method of setting bytes is inefficient, it would be
    // better if we did this with pure bit manipulation.
    logger << std::format("word: {:X}\n", word);
    std::array<int64_t, 8> bytes = {
                word & 0xFF,
        (word >>  8) & 0xFF,
        (word >> 16) & 0xFF,
        (word >> 24) & 0xFF,
        (word >> 32) & 0xFF,
        (word >> 40) & 0xFF,
        (word >> 48) & 0xFF,
        (word >> 56) & 0xFF,
    };

    bytes[byte_offset] = val;

    for (auto b : bytes) {
        logger << std::format("{:02X} ", b);
    }
    logger << " => ";

    word =      bytes[0] | 
        (bytes[1] <<  8) | 
        (bytes[2] << 16) | 
        (bytes[3] << 24) | 
        (bytes[4] << 32) |
        (bytes[5] << 40) |
        (bytes[6] << 48) |
        (bytes[7] << 56);

    for (auto b : bytes) {
        logger << std::format("{:02X} ", b);
    }
    logger << "\n";

    int res = ptrace(PTRACE_POKEDATA, _pid, (void*)aligned_addr, (void*)word);
    if (res == -1) {
        perror("ptrace POKEDATA");
        logger << strerror(errno);
        exit(EXIT_FAILURE);
    }
    detach_process();
}

void MemoryTool::write(uint16_t val, mem_addr addr) const {
    logger << std::format("writing uint16 to 0x{:X}...\n", addr);
    attach_process();
    mem_addr aligned_addr = addr & ~(WORD_SIZE - 1);
    size_t byte_offset = addr % WORD_SIZE;
    errno = 0;
    long word = ptrace(PTRACE_PEEKDATA, _pid, (void*)aligned_addr, nullptr);
    if (word == -1 && errno != 0) {
        perror("ptrace PEEKDATA");
        logger << strerror(errno);
        exit(EXIT_FAILURE);
    }
    
    std::array<int64_t, 8> bytes = {
                word & 0xFF,
        (word >>  8) & 0xFF,
        (word >> 16) & 0xFF,
        (word >> 24) & 0xFF,
        (word >> 32) & 0xFF,
        (word >> 40) & 0xFF,
        (word >> 48) & 0xFF,
        (word >> 56) & 0xFF,
    };

    bytes[byte_offset] = val & 0xFF;
    bytes[byte_offset+1] = val >> 8;

    word =      bytes[0] | 
        (bytes[1] <<  8) | 
        (bytes[2] << 16) | 
        (bytes[3] << 24) | 
        (bytes[4] << 32) |
        (bytes[5] << 40) |
        (bytes[6] << 48) |
        (bytes[7] << 56);
    logger << std::format("0x{:016X}: ", addr);
    for (auto b : bytes) {
        logger << std::format("{:02X} ", b);
    }
    logger << "\n";

    int res = ptrace(PTRACE_POKEDATA, _pid, (void*)aligned_addr, (void*)word);
    if (res == -1) {
        perror("ptrace POKEDATA");
        logger << strerror(errno);
        exit(EXIT_FAILURE);
    }
    detach_process();
}

void MemoryTool::write(uint32_t val, mem_addr addr) const {
    logger << std::format("writing uint32 to 0x{:X}...\n", addr);
    attach_process();
    mem_addr aligned_addr = addr & ~(WORD_SIZE - 1);
    size_t byte_offset = addr % WORD_SIZE;
    errno = 0;
    long word = ptrace(PTRACE_PEEKDATA, _pid, (void*)aligned_addr, nullptr);
    if (word == -1 && errno != 0) {
        perror("ptrace PEEKDATA");
        logger << std::format("0x{:x}: {:d}\n", addr, val);
        logger << strerror(errno);
        exit(EXIT_FAILURE);
    }
    
    std::array<int64_t, 8> bytes = {
        (word >>  0) & 0xFF,
        (word >>  8) & 0xFF,
        (word >> 16) & 0xFF,
        (word >> 24) & 0xFF,
        (word >> 32) & 0xFF,
        (word >> 40) & 0xFF,
        (word >> 48) & 0xFF,
        (word >> 56) & 0xFF,
    };

    bytes[byte_offset] =   (val >>  0) & 0xFF;
    bytes[byte_offset+1] = (val >>  8) & 0xFF;
    bytes[byte_offset+2] = (val >> 16) & 0xFF;
    bytes[byte_offset+3] = (val >> 24) & 0xFF;

    word =      bytes[0] | 
        (bytes[1] <<  8) | 
        (bytes[2] << 16) | 
        (bytes[3] << 24) | 
        (bytes[4] << 32) |
        (bytes[5] << 40) |
        (bytes[6] << 48) |
        (bytes[7] << 56);

    logger << std::format("0x{:016X}: ", addr);
    for (const auto& b : bytes) {
        logger << std::format("{:02X} ", b);
    }
    logger << "\n";

    int res = ptrace(PTRACE_POKEDATA, _pid, (void*)aligned_addr, (void*)word);
    if (res == -1) {
        perror("ptrace POKEDATA");
        logger << strerror(errno);
        exit(EXIT_FAILURE);
    }
    detach_process();
}

void MemoryTool::write(uint64_t val, mem_addr addr) const {
    attach_process();
    errno = 0;
    mem_addr aligned_addr = addr & ~(WORD_SIZE - 1);
    long word = ptrace(PTRACE_PEEKDATA, _pid, (void*)aligned_addr, nullptr);
    if (word == -1 && errno != 0) {
        perror("ptrace PEEKDATA");
        logger << strerror(errno);
        exit(EXIT_FAILURE);
    }
    
    std::array<uint64_t, 8> bytes = {
        (val >>  0) & 0xFF,
        (val >>  8) & 0xFF,
        (val >> 16) & 0xFF,
        (val >> 24) & 0xFF,
        (val >> 32) & 0xFF,
        (val >> 40) & 0xFF,
        (val >> 48) & 0xFF,
        (val >> 56) & 0xFF,
    };

    word =      bytes[0] | 
         (bytes[1] << 8) | 
        (bytes[2] << 16) | 
        (bytes[3] << 24) | 
        (bytes[4] << 32) |
        (bytes[5] << 40) |
        (bytes[6] << 48) |
        (bytes[7] << 56);

    logger << std::format("0x{:016X}: ", addr);
    for (auto b : bytes) {
        logger << std::format("{:02X} ", b);
    }
    logger << "\n";

    int res = ptrace(PTRACE_POKEDATA, _pid, (void*)aligned_addr, (void*)word);
    if (res == -1) {
        perror("ptrace POKEDATA");
        logger << strerror(errno);
        exit(EXIT_FAILURE);
    }
    detach_process();
}

void MemoryTool::write(float val, mem_addr addr) const {
    attach_process();
    errno = 0;
    mem_addr aligned_addr = addr & ~(WORD_SIZE - 1);
    const size_t byte_offset = addr % WORD_SIZE;

    long word = ptrace(PTRACE_PEEKDATA, _pid, (void*)aligned_addr, nullptr);
    if (word == -1 && errno != 0) {
        perror("ptrace PEEKDATA");
        logger << strerror(errno);
        exit(EXIT_FAILURE);
    }

    uint32_t float_bits = std::bit_cast<uint32_t>(val);
    uint8_t* bytes = reinterpret_cast<uint8_t*>(&word);
    std::memcpy(bytes + byte_offset, &float_bits, sizeof(float));
    logger << std::format("0x{:016X}: writing float {:f}\n", addr, val);

    int res = ptrace(PTRACE_POKEDATA, _pid, (void*)aligned_addr, (void*)word);
    if (res == -1) {
        perror("ptrace POKEDATA");
        logger << strerror(errno);
        exit(EXIT_FAILURE);
    }
    detach_process();
}

bool MemoryTool::attach_process() const {
    if (ptrace(PTRACE_ATTACH, _pid, NULL, NULL) == -1) {
        perror("ptrace ATTACH");
        logger << strerror(errno);
        return false;
    }
    waitpid(_pid, NULL, 0);
    return true;
}

std::vector<mem_addr> MemoryTool::list_search_results() const {
    std::vector<mem_addr> res;
    if (_search_results.size() == 0) {
        logger << "There are no search results...\n";
        return {};
    }
    int i = 0;
    for (const mem_addr addr : _search_results) {
        logger << std::format("{:d}. 0x{:x}\n", ++i, addr);
        res.emplace_back(addr);
    }
    return res;
}

std::vector<std::string> MemoryTool::get_search_list() const {
    std::vector<std::string> res;
    if (_search_results.size() == 0) {
        return {};
    }
    for (const mem_addr addr : _search_results) {
        res.emplace_back(std::format("0x{:x}\n", addr));
    }
    return res;
}

bool MemoryTool::detach_process() const {
    int res = ptrace(PTRACE_DETACH, _pid, nullptr, (void*)SIGCONT);
    return (res == 0);
}

void MemoryTool::clear_results() {
    _search_results = {};
    logger << "search results cleared...\n";
}

void MemoryTool::set_datatype_mode(DatatypeMode mode) {
    logger << std::format("set datatype mode to: {:s}\n", datatype_mode_string_map[mode].second);
    _datatype_mode = mode;
}

DatatypeMode MemoryTool::get_datatype_mode() const {
    return _datatype_mode;
}
