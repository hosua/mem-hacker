#include <format>
#include <iostream>
#include <sstream>

#include "mem_tool.hh"

void display_menu() {
    std::cout << "---- Memory Tool ----\n";
    std::cout << "1. Search\n";
    std::cout << "2. Clear Results\n";
    std::cout << "3. Write to memory (by list)\n";
    std::cout << "4. Write to memory (by address)\n";
    std::cout << "Enter your choice: ";
}

int main(int argc, const char** argv) {
    const uint32_t pid = strtoul(argv[1], NULL, 0);
    std::cout << pid << "\n";

    MemoryTool mem_tool;
    mem_tool.read(pid);
    
    bool running = true;
    
    int choice;
    while (running) {
        display_menu();
        std::cin >> choice;

        if (std::cin.fail()) {
            std::cout << "Invalid input. Please enter a number.\n";
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        }

        switch (choice) {
            case 1:
            {
                int val;
                std::cout << "Enter a value to search for: ";
                std::cin >> val;
                mem_tool.search(val);
                break;
            }
            case 2:
                mem_tool.clear_results();
                break;
            case 3:
                {
                    std::string input;
                    uint64_t val;
                    uint32_t addr_selection = -1;
                    mem_addr addr;
                    std::cout << "Enter a value to write: ";
                    std::cin >> input;
                    std::vector<mem_addr> addrs = mem_tool.list_search_results();
                    while (addr_selection < 0 || addr_selection > addrs.size()) {
                        std::cout << "Enter the number of the address to write to: ";
                        std::cin >> addr_selection;
                        addr = addrs[addr_selection-1];
                    }
                    mem_tool.write(val, addr);
                    break;
                }
            case 4:
                {
                    uint32_t val;
                    mem_addr addr;
                    std::cout << "Enter a value to write: ";
                    std::cin >> val;
                    std::cout << "Enter an address to write to: ";
                    std::cin >> std::hex >> addr;
                    std::cout << "addr: " << std::format("0x{:x}", addr)<< "\n";
                    mem_tool.write(val, addr);
                    break;
                }
            default:
                std::cout << "Invalid choice. Please try again.\n";
                break;
        }
    }
    
    // ptrace(PTRACE_POKEDATA, pid, (void*)0x55cbf9a2a174, 0xFFFFF);
}
