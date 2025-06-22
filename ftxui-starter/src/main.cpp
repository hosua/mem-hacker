#include <iostream>
#include <vector>
#include <filesystem>
#include <cctype>
#include <fstream>
#include <sstream>

#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <ftxui/screen/string.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>

namespace fs = std::filesystem;

bool isInteger (std::string& s) {
    for (const char& c : s) if (!std::isdigit(c)) return false;
    return true;
}

const fs::path process_path = "/proc";

std::vector<std::string> gather_procs () {
    std::vector<std::string> proc_list;
    for (const fs::directory_entry& f : fs::directory_iterator(process_path)) {
        if (f.is_directory()) {
            std::string pid = f.path().stem().string();
            std::string status_path(f.path().string() + "/status");
            if (isInteger(pid)){
                std::ifstream input_f(status_path);

                if (!input_f.is_open()) {
                    perror("ifstream");
                    return {};
                }
                std::string line;
                std::getline(input_f, line); // we only need the first line for the proc name
                std::stringstream ss(line);
                std::string _, proc_name; 
                ss >> _ >> proc_name; // discard Name: into _
                std::string proc = pid + " " + proc_name + "\n";
                proc_list.emplace_back(proc);
            }
        }
    }
    return proc_list;
}

int main() {
    using namespace ftxui;
    
    Element content = vbox();
    std::vector<Element> content_children;

    std::vector<std::string> proc_list = gather_procs();
    
    auto summary = [&] {
        Element content = vbox(content_children);
        return window(text(L" Processes "), content);
    };

    std::shared_ptr<Node> document =  //
        vbox({
            hbox({
                summary() | flex,
            }),
        });

    // Limit the size of the document to 80 char.
    document = document | size(WIDTH, LESS_THAN, 80);

    std::vector<std::string> filtered_proc_list = proc_list;
    
    std::string filter_str;

    Component filter_input = Input({
        .content = &filter_str,
        .placeholder = "search...",
        .transform = [](InputState state) {
            return state.element |= bgcolor(Color::GrayDark);
        },
        .on_change = [&] {
            proc_list = gather_procs(); // refresh the process list every time user types
            filtered_proc_list = proc_list;
            std::erase_if(filtered_proc_list, [&](std::string& s) {
                return !s.contains(filter_str);
            });
        },
    });
    
    int selected = 0;
    auto menu = Menu(&filtered_proc_list, &selected);

    Component scroller = Renderer(menu, [&] {
        return menu->Render() | vscroll_indicator | frame | border;
    });

    Component process_menu = Container::Vertical({
        filter_input, 
        scroller,
    });

    ScreenInteractive screen = ScreenInteractive::FitComponent();
    screen.Loop(process_menu);

    return EXIT_SUCCESS;
}
