#include <format>
#include <iostream>

#include "mem_tool.hh"

void display_menu() {
    std::cout << "---- Memory Tool ----\n";
    std::cout << "1. Search\n";
    std::cout << "2. Clear Results\n";
    std::cout << "3. Write to memory (by list)\n";
    std::cout << "4. Write to memory (by address)\n";
    std::cout << "\nEnter your choice: ";
}

// TODO add setting to search values by hex or decimal (even better,
// automatically treat as hex when prefixed with 0x)

// TODO add setting to switch between searching data types

// TODO multi-threading support? (big dick points)

int main(int argc, const char** argv) {
    const uint32_t pid = strtoul(argv[1], NULL, 0);
    std::cout << std::format("attaching to process {:d}...\n\n", pid);

    MemoryTool mem_tool;
    
    bool running = true;
    
    int choice;
    while (running) {
        int dump_res;
        // re-read the memory before every action
        if ( (dump_res = mem_tool.dump(pid) == -1) ){
            perror("MemoryTool.dump");
            running = false;
            continue;
        } 
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
                uint16_t val;
                std::cout << "Enter a value to search for: ";
                std::cin >> std::dec >> val;
                mem_tool.search(val);
                break;
            }
            case 2:
                mem_tool.clear_results();
                break;
            case 3:
                {
                    uint32_t val;
                    uint32_t addr_selection = -1;
                    mem_addr addr;
                    std::cout << "Enter a value to write: ";
                    std::cin >> std::dec >> val;
                    std::vector<mem_addr> addrs = mem_tool.list_search_results();
                    while (addr_selection < 0 || addr_selection > addrs.size()) {
                        std::cout << "Enter the number of the address to write to: ";
                        std::cin >> addr_selection;
                        addr = addrs[addr_selection-1];
                    }
                    std::cout << std::format("writing to address: 0x{:X}\n", addr);
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
}
