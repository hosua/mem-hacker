#include <ncurses.h>
#include <string>
#include <vector>
#include <format>
#include <iostream>
#include "mem_tool.hh"

void print_menu(WINDOW* win, int highlight, DatatypeMode datatype_mode) {
    const std::vector<std::string> options = {
        "Search",
        "Write to memory (by list)",
        "Write to memory (by address)",
        std::format("Set datatype mode (currently {:s})", datatypeModeStringMap[datatype_mode].second),
        "List Results",
        "Clear Results",
        "Exit"
    };

    werase(win);
    box(win, 0, 0);
    mvwprintw(win, 1, 2, "---- Memory Tool ----");
    for (size_t i = 0; i < options.size(); ++i) {
        if (i == highlight)
            wattron(win, A_REVERSE);
        mvwprintw(win, 3 + i, 4, options[i].c_str());
        wattroff(win, A_REVERSE);
    }
    wrefresh(win);
}

void pause_msg(const std::string& msg) {
    mvprintw(LINES - 2, 2, msg.c_str());
    mvprintw(LINES - 1, 2, "Press any key to continue...");
    getch();
    clear();
}

int main(int argc, const char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: ./mem_tool_ncurses <pid>\n";
        return EXIT_FAILURE;
    }

    const uint32_t pid = strtoul(argv[1], nullptr, 0);
    MemoryTool mem_tool;

    // ncurses setup
    initscr();
    noecho();
    cbreak();
    keypad(stdscr, TRUE);

    WINDOW* menu_win = newwin(12, 60, (LINES - 12) / 2, (COLS - 60) / 2);
    keypad(menu_win, TRUE);

    int highlight = 0;
    int choice = 0;
    bool running = true;

    while (running) {
        if (mem_tool.dump(pid) == -1) {
            endwin();
            perror("MemoryTool.dump");
            return EXIT_FAILURE;
        }

        DatatypeMode datatype_mode = mem_tool.get_datatype_mode();
        print_menu(menu_win, highlight, datatype_mode);
        int input = wgetch(menu_win);

        switch (input) {
            case KEY_UP:
                highlight = (highlight - 1 + 7) % 7;
                break;
            case KEY_DOWN:
                highlight = (highlight + 1) % 7;
                break;
            case '\n':
                choice = highlight;
                clear();

                switch (choice) {
                    case 0: // Search
                    {
                        echo();
                        mvprintw(2, 2, "Enter value to search: ");
                        if (datatype_mode == DTM_FLOAT) {
                            float val;
                            scanw("%f", &val);
                            mem_tool.search(val);
                        } else {
                            uint64_t val;
                            scanw("%lu", &val);
                            switch (datatype_mode) {
                                case DTM_U8: mem_tool.search((uint8_t)val); break;
                                case DTM_U16: mem_tool.search((uint16_t)val); break;
                                case DTM_U32: mem_tool.search((uint32_t)val); break;
                                case DTM_U64: mem_tool.search((uint64_t)val); break;
                                default:
                                    pause_msg("Datatype not implemented, falling back to uint32_t.");
                                    mem_tool.search((uint32_t)val);
                            }
                        }
                        noecho();
                        pause_msg("Search complete.");
                        break;
                    }
                    case 1: // Write (by list)
                    {
                        echo();
                        mvprintw(2, 2, "Enter value to write: ");
                        uint64_t val;
                        scanw("%lu", &val);

                        {
                            std::vector<mem_addr> addrs = mem_tool.list_search_results();
                            for (size_t i = 0; i < addrs.size(); ++i) {
                                mvprintw(4 + i, 4, "%lu. 0x%lx", i + 1, addrs[i]);
                            }

                            mvprintw(4 + addrs.size() + 1, 2, "Select address #: ");
                            int idx;
                            scanw("%d", &idx);
                            mem_addr target = addrs[idx - 1];

                            switch (datatype_mode) {
                                case DTM_U8: mem_tool.write((uint8_t)val, target); break;
                                case DTM_U16: mem_tool.write((uint16_t)val, target); break;
                                case DTM_U32: mem_tool.write((uint32_t)val, target); break;
                                case DTM_U64: mem_tool.write((uint64_t)val, target); break;
                                default:
                                    pause_msg("Datatype not implemented, fallback to uint32_t.");
                                    mem_tool.write((uint32_t)val, target);
                            }
                        }
                        noecho();
                        pause_msg("Write complete.");
                        break;
                    }
                    case 2: // Write (by address)
                    {
                        echo();
                        mvprintw(2, 2, "Enter value to write: ");
                        if (datatype_mode == DTM_FLOAT) {
                            float val;
                            scanw("%f", &val);
                            mem_addr addr;
                            mvprintw(3, 2, "Enter address (hex): ");
                            scanw("%lx", &addr);
                            mem_tool.write(val, addr);
                        } else {
                            uint64_t val;
                            scanw("%lu", &val);
                            mem_addr addr;
                            mvprintw(3, 2, "Enter address (hex): ");
                            scanw("%lx", &addr);
                            switch (datatype_mode) {
                                case DTM_U8: mem_tool.write((uint8_t)val, addr); break;
                                case DTM_U16: mem_tool.write((uint16_t)val, addr); break;
                                case DTM_U32: mem_tool.write((uint32_t)val, addr); break;
                                case DTM_U64: mem_tool.write((uint64_t)val, addr); break;
                                default: mem_tool.write((uint32_t)val, addr);
                            }
                        }
                        noecho();
                        pause_msg("Write complete.");
                        break;
                    }
                    case 3: // Set datatype
                    {
                        echo();
                        int i = 0;
                        for (const auto& [mode, str] : datatypeModeStringMap) {
                            mvprintw(2 + i, 4, "%d. %s", ++i, str.c_str());
                        }
                        mvprintw(3 + i, 2, "Select datatype #: ");
                        int opt;
                        scanw("%d", &opt);
                        mem_tool.set_datatype_mode(datatypeModeStringMap[opt - 1].first);
                        noecho();
                        break;
                    }
                    case 4: // List results
                    {
                        mem_tool.list_search_results();  // You’ll need to redirect output
                        pause_msg("Listed results.");
                        break;
                    }
                    case 5: // Clear results
                    {
                        mem_tool.clear_results();
                        pause_msg("Results cleared.");
                        break;
                    }
                    case 6: // Exit
                        running = false;
                        break;
                }
                break;
        }
    }

    delwin(menu_win);
    endwin();
    return 0;
}
