#include <format>
#include <iostream>

#include "mem_tool.hh"

void display_menu(DatatypeMode datatype_mode) {
    std::cout << "\n---- Memory Tool ----\n"
              << "1. Search\n"
              << "2. Write to memory (by list)\n"
              << "3. Write to memory (by address)\n"
              << std::format("4. Set datatype mode (currently set to {:s})\n", 
                             datatypeModeStringMap[datatype_mode].second)
              << "5. List Results\n"
              << "6. Clear Results\n"
              << "\nEnter your choice: ";
}

int main(int argc, const char** argv) {
    const uint32_t pid = strtoul(argv[1], NULL, 0);
    std::cout << std::format("attaching to process {:d}...\n\n", pid);

    MemoryTool mem_tool;
    
    bool running = true;
    
    while (running) {
        int choice;
        int dump_res;
        // update memory dump before every action
        if ( (dump_res = mem_tool.dump(pid) == -1) ){
            perror("MemoryTool.dump");
            running = false;
            continue;
        } 

        DatatypeMode datatype_mode = mem_tool.get_datatype_mode();
        std::string datatype_mode_str = datatypeModeStringMap[datatype_mode].second;
        display_menu(datatype_mode);
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
                switch (datatype_mode) {
                    case DTM_U8:
                        mem_tool.search((uint8_t)val);
                        break;
                    case DTM_U16:
                        mem_tool.search((uint16_t)val);
                        break;
                    default:
                        std::cout << std::format("Warning: datatype mode {:s} not yet implemented for searching, falling back to uint16_t...\n", 
                                                 datatype_mode_str);
                        mem_tool.search((uint16_t)val);
                        break;
                }
                break;
            }
            case 2:
                {
                    uint32_t val;
                    uint32_t addr_selection = -1;
                    mem_addr addr;
                    std::cout << "Enter a value to write: ";
                    std::cin >> std::dec >> val;
                    std::vector<mem_addr> addrs = mem_tool.list_search_results();
                    while (addr_selection <= 0 || addr_selection > addrs.size()) {
                        std::cout << "Enter the number of the address to write to: ";
                        std::cin >> addr_selection;
                        if (addr_selection <= 0 || addr_selection > addrs.size()) {
                            std::cout << "Invalid option selected...\n";
                        }
                    }
                    addr = addrs[addr_selection-1];
                    switch (datatype_mode) {
                        case DTM_U8:
                            mem_tool.write((uint8_t)val, addr);
                            break;
                        case DTM_U16:
                            mem_tool.write((uint16_t)val, addr);
                            break;
                        case DTM_U32:
                            mem_tool.write((uint32_t)val, addr);
                            break;
                        case DTM_U64:
                            mem_tool.write((uint64_t)val, addr);
                            break;
                        default:
                            std::cout << std::format("Warning: datatype mode {:s} not yet implemented, falling back to uint32_t...\n", 
                                                     datatype_mode_str);
                            mem_tool.write((uint32_t)val, addr);
                            break;
                    }
                    break;
                }
            case 3:
                {
                    uint32_t val;
                    mem_addr addr;
                    std::cout << "Enter a value to write: ";
                    std::cin >> val;
                    std::cout << "Enter an address to write to: ";
                    std::cin >> std::hex >> addr;
                    switch (datatype_mode) {
                        case DTM_U8:
                            mem_tool.write((uint8_t)val, addr);
                            break;
                        case DTM_U16:
                            mem_tool.write((uint16_t)val, addr);
                            break;
                        case DTM_U32:
                            mem_tool.write((uint32_t)val, addr);
                            break;
                        case DTM_U64:
                            mem_tool.write((uint64_t)val, addr);
                            break;
                        default:
                            std::cout << std::format("Warning: datatype mode {:s} not yet implemented, falling back to uint32_t...\n", 
                                                     datatype_mode_str);
                            mem_tool.write((uint32_t)val, addr);
                            break;
                    }
                    break;
                }
            case 4:
                { 
                    uint32_t choice_opt = -1;
                    int i = 0;
                    for (const auto& [mode, str] : datatypeModeStringMap)
                        std::cout <<  std::format("{:d}. {:s}\n", ++i, str);
                    while (choice_opt <= 0 || choice_opt > datatypeModeStringMap.size()) {
                        std::cout << std::endl << "Enter the number to select the datatype you wish you read/write: ";
                        std::cin >> choice_opt; 
                        if (choice_opt <= 0 || choice_opt > datatypeModeStringMap.size()) {
                            std::cout << "Invalid option selected...\n";
                        }
                    }
                    DatatypeMode mode = datatypeModeStringMap[choice_opt-1].first;
                    mem_tool.set_datatype_mode(mode);
                    break;
                }
            case 5: 
                mem_tool.list_search_results();
                break;
            case 6:
                mem_tool.clear_results();
                break;
            default:
                std::cout << "Invalid choice. Please try again.\n";
                break;
        }
    }
}
