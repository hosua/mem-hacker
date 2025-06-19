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

int MemoryTool::read(int pid) {
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
        memory = std::vector<byte>(n_bytes, 0);
        pread(fd, (void*)memory.data(), n_bytes, addr_start);

        // int i = 0;
        // fprintf(stdout, "REGION: %lx - %lx\n", addr_start, addr_end);
        // for (const byte& mem : memory) {
        //     if (i % 8 == 0)
        //         fprintf(stdout, "%016lx: ", addr_start + i);

        //     fprintf(stdout, "%02x ", mem);

        //     if (++i % 8 == 0)
        //         fprintf(stdout, "\n");
        // }

        // if (i % 8 != 0)
        //     fprintf(stdout, "\n");  
    }

    return 0;
}

bool MemoryTool::search(byte val) {
    attach_process();
    // clean search
    if (_search_results.size() == 0) {
        for (const auto& [region, bytes] : _mem) {
            std::vector<std::string> start_end = split_string(region, "-");
            const mem_addr addr_start = std::stoull(start_end[0], nullptr, 16),
                           addr_end = std::stoull(start_end[1], nullptr, 16);
            
            int offset = 0;
            for (const byte& b : bytes) {
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
            byte b = read_byte_at(addr);
            if (b == val) std::cout << std::format("0x{:x}: {:d}\n", addr, b);
            return b != val;
        });
        std::cout << _search_results.size() << " results remain.\n";
    }
    detach_process();
    return true;
}

byte MemoryTool::read_byte_at(mem_addr addr) {
    mem_addr aligned_addr = addr & ~(WORD_SIZE - 1);
    word data = ptrace(PTRACE_PEEKDATA, _pid, (void*)aligned_addr, nullptr);
    if (data == -1) {
        perror("ptrace PEEKDATA");
        return -1;
    }
    const size_t byte_offset = addr % WORD_SIZE;
    byte b = (data >> (8 * byte_offset)) & 0xFF;
    return b;
}

bool MemoryTool::attach_process() const {
    if (ptrace(PTRACE_ATTACH, _pid, NULL, NULL) == -1) {
        perror("ptrace ATTACH");
        return false;
    }
    waitpid(_pid, NULL, 0);
    return true;
}

bool MemoryTool::detach_process() const {
    int res = ptrace(PTRACE_DETACH, _pid, nullptr, (void*)SIGCONT);
    return (res == 0);
}

void MemoryTool::clear_results() {
    _mem = {};
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

void MemoryTool::write(uint8_t val, uint64_t addr) const {
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
