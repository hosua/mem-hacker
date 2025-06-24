#include "mem_tool_menu.hh"
#include "mem_tool.hh"

#include <ftxui/dom/elements.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>

#include <ftxui/screen/screen.hpp>
#include <string>
#include <vector>
#include <iostream>

static int selected_result;
static std::vector<std::string> results_list = {};
static DatatypeMode datatype_mode = DTM_I32;

static bool is_searching = false;
static std::atomic<size_t> spinner_frame = 0;
// static int spinner_charset = 7;
static int spinner_charset = 8;
std::atomic<bool> spinner_running = true;

static MemoryTool mem_tool;

static uint64_t val_int; // all int types can be casted from uint64_t
static float val_float;
static double val_double;

using namespace ftxui;

ScreenInteractive screen = ScreenInteractive::Fullscreen();

static void run_search(const std::string& search_str, int pid) {
    is_searching = true;
    int dump_res;
    if ( (dump_res = mem_tool.dump(pid) == -1) ){
        perror("MemoryTool.dump");
        return;
    }

    if (datatype_mode == DTM_FLOAT) {
        val_float = std::stof(search_str);
        mem_tool.search(val_float);
    } 
    // else if (datatype_mode == DTM_DOUBLE) {
    //     // TODO double isn't implemented yet!
    //     val_double = std::stod(search_str);
    //     double val;
    //     mem_tool.search(val);
    // } 
    else {
        val_int = std::stod(search_str);
        switch (datatype_mode) {
            case DTM_I8:  mem_tool.search((uint8_t)val_int); break;
            case DTM_I16: mem_tool.search((uint16_t)val_int); break;
            case DTM_I32: mem_tool.search((uint32_t)val_int); break;
            case DTM_I64: mem_tool.search((uint64_t)val_int); break;
            case DTM_DOUBLE: case DTM_FLOAT: break;
        }
    }
    results_list = mem_tool.get_search_list();
    is_searching = false;
    screen.PostEvent(ftxui::Event::Custom); // force redraw
}

namespace MemToolMenu {
    void run(int pid) {
        int dump_res;
        if ( (dump_res = mem_tool.dump(pid) == -1) ){
            perror("MemoryTool.dump");
            return;
        }

        Component results_menu = 
            Menu(&results_list, &selected_result);

        bool writing_mode = false; // when true, display write to address UI
        uint64_t addr_to_write_to = 0; // the address to write values to in write mode
        results_menu |= CatchEvent([&](Event event) {
            if (Event::Character('\n') == event) {
                if (results_list.empty()) return false;
                std::string addr_str = results_list[selected_result];
                addr_str.erase(0, 2);
                addr_to_write_to = std::stoull(addr_str);
                writing_mode = true;
                return true;
            }
            return false;
        });

        Component loading_spinner = Renderer([&] {
            if (!is_searching) return text(" ");
            return spinner(spinner_charset, spinner_frame);
        });

        std::thread spinner_thread([&] {
            while (spinner_running) {
                if (is_searching) {
                    spinner_frame++;
                    screen.PostEvent(Event::Custom); // Triggers UI refresh
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        });

        std::string search_str;
        Component search_input = Input({
            .content = &search_str,
            .placeholder = "enter a value to search...",
            .transform = [](InputState state) {
                return state.element |= bgcolor(Color());
                // return state.element |= bgcolor(Color::GrayDark);
            },
            .multiline = false,
            .on_enter = [&] {
                if (search_str.empty() || is_searching) return;
                std::thread([=] {
                    run_search(search_str, pid);
                }).detach();
                search_str.clear();
            },
        });
        
        search_input |= CatchEvent([&](Event event) {
            if (!event.is_character()) return false;
            char ch = event.character()[0];
            // only allow user to type . if one does not already exist, and if  double or float are selected
            if ((datatype_mode == DTM_FLOAT || datatype_mode == DTM_DOUBLE) && !search_str.contains("."))
                return !std::isdigit(ch) && ch != '.';
            return !std::isdigit(ch);

        });

        Component search_btn = Button("Search", [&] {
            if (search_str.empty()) return;
            run_search(search_str, pid);
            search_str.clear();
        });

        Component clear_btn = Button("Clear", [&] {
            mem_tool.clear_results();
            results_list.clear();
        });
        
        int selected_datatype_index = 2, prev_datatype_index = selected_datatype_index;
        std::vector<std::string> dropdown_opts;
        for (const auto& [datatype_mode, str] : datatype_mode_string_map)
            dropdown_opts.emplace_back(str);

        Component datatype_dropdown = Dropdown(dropdown_opts, &selected_datatype_index)
            | CatchEvent([&](Event event){
                datatype_mode = datatype_mode_string_map[selected_datatype_index].first;
                search_str.clear();
                return false;
            });

        Component bottom_section_container = Container::Horizontal({
            Container::Horizontal({ search_btn, clear_btn }) | xflex, 
            datatype_dropdown | align_right | xflex
        });

        std::string write_value_str;

        Component write_value_input = Input({
            .content = &write_value_str,
            .placeholder = std::format("enter a value to write, or press esc to cancel", addr_to_write_to),
            .transform = [](InputState state){ return state.element |= bgcolor(Color()); },
            .multiline = false,
            .on_enter = [&] {
                std::string addr_str = results_list[selected_result];
                std::stringstream ss;
                ss << std::hex << addr_str;
                mem_addr addr;
                ss >> addr;
                
                if (!write_value_str.empty()) {
                    switch (datatype_mode) {
                        case DTM_I8:
                            val_int = std::stoi(write_value_str); 
                            mem_tool.write((uint8_t)val_int, addr);
                        break;
                        case DTM_I16:
                            val_int = std::stoi(write_value_str); 
                            mem_tool.write((uint16_t)val_int, addr);
                        break;
                        case DTM_I32:
                            val_int = std::stoi(write_value_str); 
                            mem_tool.write((uint32_t)val_int, addr);
                        break;
                        case DTM_I64:
                            val_int = std::stoull(write_value_str); 
                            mem_tool.write((uint64_t)val_int, addr);
                        break;
                        case DTM_FLOAT:
                            val_float = std::stof(write_value_str); 
                            mem_tool.write(val_float, addr);
                        break;
                        case DTM_DOUBLE: // TODO: Not yet implemented
                        break;
                    }
                    // uint32_t write_value = std::stoull(write_value_str); 
                    // mem_tool.write(write_value, addr);
                    writing_mode = false;
                    write_value_str.clear();
                }
            },
        }) | border;

        write_value_input |= CatchEvent([&](Event event) {
            if (event == Event::Escape) writing_mode = false;
            if (!event.is_character()) return false;
            char ch = event.character()[0];
            // allow - for first char
            if (ch == '-' && write_value_str.empty()) return false;
            // allow single dot
            if ((datatype_mode == DTM_FLOAT || datatype_mode == DTM_DOUBLE) &&
                ch == '.' && write_value_str.find('.') == std::string::npos) {
                return false; 
            }
            return !std::isdigit(ch);
        });

        Component search_section = Container::Horizontal({
            loading_spinner | vcenter,
            search_input | border | size(WIDTH, GREATER_THAN, 40),
        });
        
        Component padded_search_section = Renderer([&] {
            return hbox({
                search_section->Render(),
                text(" "),
            });
        }) | CatchEvent([&](Event event) { return search_section->OnEvent(event); });

        Component search_container = Container::Vertical({
            padded_search_section,
            results_menu | vscroll_indicator | frame | border | size(HEIGHT, GREATER_THAN, 3) | yflex,
            Maybe(write_value_input, &writing_mode),
            bottom_section_container,
        }) | CatchEvent([&](Event event) { return false; });

        Component search_window = Window({ 
            .inner = search_container,
            .title = " mem-hacker ",
            .width = 200,
            .height = 180,
            .resize_left = false,
            .resize_right = false,
            .resize_top = false,
            .resize_down = false,
        });

        Component master_container = Container::Stacked({
            search_window,
        });
        
        // When in writing mode, force keyboard events to the write input 
        master_container |= CatchEvent([&](Event event) {
            if (writing_mode) return write_value_input->OnEvent(event);
            return search_window->OnEvent(event);
        });

        screen.Loop(master_container);
        spinner_running = false;
        spinner_thread.join();
    }
}
