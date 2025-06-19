#include "mem_tool.hh"

#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <unistd.h>
#include <format>
#include <fstream>
#include <iostream>

static const size_t WORD_SIZE = sizeof(long);

static std::vector<std::string> split_string(const std::string& str, const std::string& delim) {
    std::vector<std::string> res;
    std::string s = str;
    size_t pos = 0;
    while ((pos = s.find(delim)) != std::string::npos) {
        std::string tok = s.substr(0, s.find(delim));
        res.push_back(s.substr(0, s.find(delim)));
        s.erase(0, pos + delim.length());
    }
    res.push_back(s);
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
        return -1;
    }
    
    std::string mem_map_file = "/proc/" + std::to_string(_pid) + "/maps";
    std::ifstream file(mem_map_file, std::ios::in);
    if (file.is_open()) {
        std::string line;
        while (std::getline(file, line)) {
            if (line.find("rw-p") != std::string::npos &&     // get readable/writable private memory
                line.find("/usr/lib") == std::string::npos && // ignore libraries
                line.find("[stack]") == std::string::npos     // ignore the stack
            ) {
                // parse start and end address;
                std::vector<std::string> start_end = split_string(split_string(line, " ")[0], "-");
                const mem_addr addr_start = std::stoull(start_end[0], nullptr, 16), 
                               addr_end = std::stoull(start_end[1], nullptr, 16);
                _mem[std::string(std::format("{:x}", addr_start) + "-" + std::format("{:x}", addr_end))] = {};
                // std::cout << addr_start <<  " to "  << addr_end << "\n";
            }
        }
    } else {
        perror("ifstream");
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
    uint64_t lo_word = ptrace(PTRACE_PEEKDATA, _pid, (void*)aligned_addr, nullptr);
    if (lo_word == -1ULL && errno != 0) {
        perror("ptrace PEEKDATA");
        return 0;
    }

    if (byte_offset <= WORD_SIZE - sizeof(uint16_t)) {
        return (lo_word >> (8 * byte_offset)) & 0xFFFF;
    }

    // Crosses boundary — need next word
    errno = 0;
    uint64_t hi_word = ptrace(PTRACE_PEEKDATA, _pid, (void*)(aligned_addr + WORD_SIZE), nullptr);
    if (hi_word == -1ULL && errno != 0) {
        perror("ptrace PEEKDATA (next word)");
        return 0;
    }

    uint8_t b0 = (lo_word >> (8 * byte_offset)) & 0xFF;
    uint8_t b1 = hi_word & 0xFF;
    return static_cast<uint16_t>(b0 | (b1 << 8));
}

/* SEARCH METHODS */

bool MemoryTool::search(uint8_t val) {
    std::cout << "searching uint8_t...\n";
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
        // std::cout << "SEARCH RESULTS\n";
        // for (const mem_addr addr : _search_results) {
        //     std::cout << std::format("0x{:x}", addr) << "\n";
        // }
        std::cout << "Found " << _search_results.size() << " results.\n";
    } else {
        // search existing
        std::erase_if(_search_results, [&](const mem_addr& addr) {
            uint8_t byte = read_uint8_at(addr);
            if (byte == val) std::cout << std::format("0x{:x}: {:d}\n", addr, byte);
            return byte != val;
        });
        std::cout << _search_results.size() << " results remain.\n";
    }
    detach_process();
    return true;
}

bool MemoryTool::search(uint16_t val) {
    std::cout << "searching uint16_t...\n";
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
        // std::cout << "SEARCH RESULTS\n";
        // for (const mem_addr& addr : _search_results) {
        //     std::cout << std::format("0x{:x}: {:d}", addr, read_uint16_at(addr)) << "\n";
        // }
        std::cout << "Found " << _search_results.size() << " results.\n";
    } else {
        // search existing
        std::erase_if(_search_results, [&](const mem_addr& addr) {
            uint16_t byte = read_uint16_at(addr);
            if (byte == val) std::cout << std::format("0x{:x}: {:d}\n", addr, byte);
            return byte != val;
        });
        std::cout << _search_results.size() << " results remain.\n";
    }
    detach_process();
    return true;
}

/* WRITE METHODS */

void MemoryTool::write(uint8_t val, uint64_t addr) const {
    std::cout << std::format("writing uint8 to 0x{:X}...\n", addr);
    attach_process();
    mem_addr aligned_addr = addr & ~(WORD_SIZE - 1);
    size_t byte_offset = addr % WORD_SIZE;
    errno = 0;
    long word = ptrace(PTRACE_PEEKDATA, _pid, (void*)aligned_addr, nullptr);
    if (word == -1 && errno != 0) {
        perror("ptrace PEEKDATA");
        exit(EXIT_FAILURE);
    }
    
    std::cout << "byte offset: " << byte_offset << "\n";
    std::cout << std::format("word: {:X}\n", word);
    std::vector<uint64_t> bytes = {
        static_cast<unsigned long>((word) & 0xFF),
        static_cast<unsigned long>((word >> 8) & 0xFF),
        static_cast<unsigned long>((word >> 16) & 0xFF),
        static_cast<unsigned long>((word >> 24) & 0xFF),
        static_cast<unsigned long>((word >> 32) & 0xFF),
        static_cast<unsigned long>((word >> 40) & 0xFF),
        static_cast<unsigned long>((word >> 48) & 0xFF),
        static_cast<unsigned long>((word >> 56) & 0xFF),
    };

    for (auto b : bytes) {
        std::cout << std::format("{:02X} ", b);
    }
    std::cout << " => ";

    word = 
        bytes[0] | 
        (bytes[1] << 8) | 
        (bytes[2] << 16) | 
        (bytes[3] << 24) | 
        (bytes[4] << 32) |
        (bytes[5] << 40) |
        (bytes[6] << 48) |
        (bytes[7] << 56);
    // std::cout << std::format("word: {:X}\n", word);
    for (auto b : bytes) {
        std::cout << std::format("{:02X} ", b);
    }
    std::cout << "\n";

    int res = ptrace(PTRACE_POKEDATA, _pid, (void*)aligned_addr, (void*)word);
    if (res == -1) {
        perror("ptrace POKEDATA");
        exit(EXIT_FAILURE);
    }
    detach_process();
}

void MemoryTool::write(uint16_t val, uint64_t addr) const {
    std::cout << std::format("writing uint16 to 0x{:X}...\n", addr);
    attach_process();
    mem_addr aligned_addr = addr & ~(WORD_SIZE - 1);
    size_t byte_offset = addr % WORD_SIZE;
    errno = 0;
    long word = ptrace(PTRACE_PEEKDATA, _pid, (void*)aligned_addr, nullptr);
    if (word == -1 && errno != 0) {
        perror("ptrace PEEKDATA");
        exit(EXIT_FAILURE);
    }
    
    std::vector<int64_t> bytes = {
        (word & 0xFF),
        ((word >> 8) & 0xFF),
        ((word >> 16) & 0xFF),
        ((word >> 24) & 0xFF),
        ((word >> 32) & 0xFF),
        ((word >> 40) & 0xFF),
        ((word >> 48) & 0xFF),
        ((word >> 56) & 0xFF),
    };

    bytes[byte_offset] = val & 0xFF;
    bytes[byte_offset+1] = val >> 8;

    word = 
        bytes[0] | 
        (bytes[1] << 8) | 
        (bytes[2] << 16) | 
        (bytes[3] << 24) | 
        (bytes[4] << 32) |
        (bytes[5] << 40) |
        (bytes[6] << 48) |
        (bytes[7] << 56);
    std::cout << std::format("0x{:016X}: ", addr);
    for (auto b : bytes) {
        std::cout << std::format("{:02X} ", b);
    }
    std::cout << std::endl;

    int res = ptrace(PTRACE_POKEDATA, _pid, (void*)aligned_addr, (void*)word);
    if (res == -1) {
        perror("ptrace POKEDATA");
        exit(EXIT_FAILURE);
    }
    detach_process();
}

void MemoryTool::write(uint32_t val, uint64_t addr) const {
    std::cout << std::format("writing uint32 to 0x{:X}...\n", addr);
    attach_process();
    mem_addr aligned_addr = addr & ~(WORD_SIZE - 1);
    size_t byte_offset = addr % WORD_SIZE;
    errno = 0;
    long word = ptrace(PTRACE_PEEKDATA, _pid, (void*)aligned_addr, nullptr);
    if (word == -1 && errno != 0) {
        perror("ptrace PEEKDATA");
        exit(EXIT_FAILURE);
    }
    
    std::vector<int64_t> bytes = {
        ((word >>  0) & 0xFF),
        ((word >>  8) & 0xFF),
        ((word >> 16) & 0xFF),
        ((word >> 24) & 0xFF),
        ((word >> 32) & 0xFF),
        ((word >> 40) & 0xFF),
        ((word >> 48) & 0xFF),
        ((word >> 56) & 0xFF),
    };

    bytes[byte_offset] = val & 0xFF;
    bytes[byte_offset+1] = val >> 8;
    bytes[byte_offset+2] = val >> 16;
    bytes[byte_offset+3] = val >> 24;

    word = 
        bytes[0] | 
        (bytes[1] << 8) | 
        (bytes[2] << 16) | 
        (bytes[3] << 24) | 
        (bytes[4] << 32) |
        (bytes[5] << 40) |
        (bytes[6] << 48) |
        (bytes[7] << 56);

    std::cout << std::format("0x{:016X}: ", addr);
    for (auto b : bytes) {
        std::cout << std::format("{:02X} ", b);
    }
    std::cout << std::endl;

    int res = ptrace(PTRACE_POKEDATA, _pid, (void*)aligned_addr, (void*)word);
    if (res == -1) {
        perror("ptrace POKEDATA");
        exit(EXIT_FAILURE);
    }
    detach_process();
}

void MemoryTool::write(uint64_t val, uint64_t addr) const {
    attach_process();
    errno = 0;
    mem_addr aligned_addr = addr & ~(WORD_SIZE - 1);
    long word = ptrace(PTRACE_PEEKDATA, _pid, (void*)aligned_addr, nullptr);
    if (word == -1 && errno != 0) {
        perror("ptrace PEEKDATA");
        exit(EXIT_FAILURE);
    }
    
    std::vector<uint64_t> bytes = {
        (val >>  0) & 0xFF,
        (val >>  8) & 0xFF,
        (val >> 16) & 0xFF,
        (val >> 24) & 0xFF,
        (val >> 32) & 0xFF,
        (val >> 40) & 0xFF,
        (val >> 48) & 0xFF,
        (val >> 56) & 0xFF,
    };

    word = 
        bytes[0] | 
        (bytes[1] << 8) | 
        (bytes[2] << 16) | 
        (bytes[3] << 24) | 
        (bytes[4] << 32) |
        (bytes[5] << 40) |
        (bytes[6] << 48) |
        (bytes[7] << 56);

    std::cout << std::format("0x{:016X}: ", addr);
    for (auto b : bytes) {
        std::cout << std::format("{:02X} ", b);
    }
    std::cout << std::endl;

    int res = ptrace(PTRACE_POKEDATA, _pid, (void*)aligned_addr, (void*)word);
    if (res == -1) {
        perror("ptrace POKEDATA");
        exit(EXIT_FAILURE);
    }
    detach_process();
}

bool MemoryTool::attach_process() const {
    if (ptrace(PTRACE_ATTACH, _pid, NULL, NULL) == -1) {
        perror("ptrace ATTACH");
        return false;
    }
    waitpid(_pid, NULL, 0);
    return true;
}

std::vector<mem_addr> MemoryTool::list_search_results() const {
    std::vector<mem_addr> res;
    int i = 0;
    for (const mem_addr addr : _search_results) {
        std::cout << std::format("{:d}. 0x{:x}\n", ++i, addr);
        res.push_back(addr);
    }
    return res;
}

bool MemoryTool::detach_process() const {
    int res = ptrace(PTRACE_DETACH, _pid, nullptr, (void*)SIGCONT);
    return (res == 0);
}

void MemoryTool::clear_results() {
    _search_results = {};
    std::cout << "search results cleared...\n";
}

