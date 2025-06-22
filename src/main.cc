#include <iostream>

#include "select_process_menu.hh"
#include "mem_tool_menu.hh"

int main() {
    int pid = SelectProcessMenu::get_pid();
    std::cout << std::format("Attaching to process: {:d}\n", pid);
    MemToolMenu::run(pid);
    return EXIT_SUCCESS;
}
