#include <iostream>
#include <fstream>

#include "select_process_menu.hh"
#include "mem_tool_menu.hh"
#include "logger.hh"

int main() {
    int pid = SelectProcessMenu::get_pid();
    MemToolMenu::run(pid);
    logger << std::format("Attaching to process: {:d}\n", pid);
    return EXIT_SUCCESS;
}
