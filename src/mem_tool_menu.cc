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
static DatatypeMode datatype_mode = DTM_U32;

static MemoryTool mem_tool;

static uint64_t val_int;
static float val_float;
static double val_double;

static void run_search(const std::string& search_str, int pid) {
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
    //     // double isn't implemented yet!
    //     val_double = std::stod(search_str);
    //     double val;
    //     mem_tool.search(val);
    // } 
    else {
        val_int = std::stod(search_str);
        switch (datatype_mode) {
            case DTM_U8:
                mem_tool.search((uint8_t)val_int);
                break;
            case DTM_U16:
                mem_tool.search((uint16_t)val_int);
                break;
            case DTM_U32:
                mem_tool.search((uint32_t)val_int);
                break;
            case DTM_U64:
                mem_tool.search((uint64_t)val_int);
                break;
            default: // default to uint32_t as failsafe
                mem_tool.search((uint32_t)val_int);
                break;
        }
    }
}

namespace MemToolMenu {
    using namespace ftxui;
    
    void run(int pid) {
        int dump_res;
        if ( (dump_res = mem_tool.dump(pid) == -1) ){
            perror("MemoryTool.dump");
            return;
        }
        ScreenInteractive screen = ScreenInteractive::Fullscreen();

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
                // TODO: need to put a write to address UI here somehow
                return true;
            }
            return false;
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
                if (search_str.empty()) return;
                run_search(search_str, pid);
                search_str.clear();
                results_list = mem_tool.get_search_list();
            },
        });
        
        search_input |= CatchEvent([&](Event event) {
            if (!event.is_character()) return false;
            char ch = event.character()[0];
            // only allow user to type . if one does not already exist, and if  double or float are selected
            if ((datatype_mode == DTM_FLOAT || datatype_mode == DTM_DOUBLE) && !search_str.contains("."))
                return !std::isdigit(ch) && ch != '.' ;
            return !std::isdigit(ch);
        });

        Component search_btn = Button("Search", [&] {
            if (search_str.empty()) return;
            run_search(search_str, pid);
            search_str.clear();
            results_list = mem_tool.get_search_list();
        });

        Component clear_btn = Button("Clear", [&] {
            mem_tool.clear_results();
            results_list = mem_tool.get_search_list();
        });
        
        int selected_datatype_index = 2, prev_datatype_index = selected_datatype_index;
        std::vector<std::string> dropdown_opts;
        for (const auto& [datatype_mode, str] : datatype_mode_string_map)
            dropdown_opts.emplace_back(str);

        Component datatype_dropdown = Dropdown(dropdown_opts, &selected_datatype_index);

        Component bottom_section_container = Container::Horizontal({
            Container::Horizontal({ search_btn, clear_btn }) | xflex, 
            datatype_dropdown | align_right | xflex
        });

        // Component search_window = Renderer(search_container, [&] {
        //     return window(text(" Search "), search_container->Render());
        // }) | CatchEvent([&](Event event){
        //     return search_container->OnEvent(event);
        // });

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
                    // TODO: handle types when writing values, assuming uint32_t for now
                    uint32_t write_value = std::stoi(write_value_str); 
                    mem_tool.write(write_value, addr);
                    writing_mode = false;
                    write_value_str.clear();
                }
            },
        }) | border;

        write_value_input |= CatchEvent([&](Event event) {
            if (event == Event::Escape) writing_mode = false;
            return false;
        });

        Component search_container = Container::Vertical({
            search_input | border,
            results_menu | vscroll_indicator | frame | border | size(HEIGHT, GREATER_THAN, 3) | yflex,
            Maybe(write_value_input, &writing_mode),
            bottom_section_container,
        }) | CatchEvent([&](Event event) { return false; });

        Component search_window = Window({ 
            .inner = search_container,
            .title = "Search",
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

        master_container |= CatchEvent([&](Event event) {
            if (writing_mode) return write_value_input->OnEvent(event);
            return search_window->OnEvent(event);
        });

        screen.Loop(master_container);
    }
}
