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
        // for (const mem_addr& addr : _search_results) {
        //     byte b = read_byte_at(addr);
        //     std::cout << "read byte: " 
        //         << std::format("{:d}", b) 
        //         << " at " 
        //         << std::format("0x{:x}", addr) 
        //         << "\n";
        // }
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
    // std::cout << std::format("{:x}", data) << "\n";
    const size_t byte_offset = addr % WORD_SIZE;
    byte b = (data >> (8 * byte_offset)) & 0xFF;
    // std::cout << std::format("{:d}", b) << "\n";
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

void MemoryTool::write(byte val, uint64_t addr) const {
    attach_process();
    mem_addr aligned_addr = addr & ~(WORD_SIZE - 1);
    int res = ptrace(PTRACE_POKEDATA, _pid, (void*)addr, (void*)(long)val);
    if (res == -1) {
        perror("ptrace POKEDATA");
        exit(EXIT_FAILURE);
    }
    detach_process();
}

void MemoryTool::clear_results() {
    _mem.clear();
}
