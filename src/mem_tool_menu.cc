#include "mem_tool_menu.hh"
#include "mem_tool.hh"

#include <ftxui/dom/elements.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>

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

static void run_search(const std::string& search_str) {
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
        
        /* ======= BEGIN SEARCH COMPONENTS ======= */

        auto results_menu = 
            Menu(&results_list, &selected_result);

        std::string search_str;
        Component search_input = Input({
            .content = &search_str,
            .placeholder = "enter a value to search...",
            .transform = [](InputState state) {
                return state.element |= bgcolor(Color::GrayDark);
            },
            .multiline = false,
            .on_enter = [&] {
                if (search_str.empty()) return;
                run_search(search_str);
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
            run_search(search_str);
            search_str.clear();
            results_list = mem_tool.get_search_list();
        });

        Component clear_btn = Button("Clear", [&] {
            mem_tool.clear_results();
            results_list = mem_tool.get_search_list();
        });

        Component search_btn_container = Container::Horizontal({
            search_btn, clear_btn,
        });

        Component search_container = Container::Vertical({
            search_input | border,
            results_menu | vscroll_indicator | frame | border | size(HEIGHT, GREATER_THAN, 3) | yflex,
            search_btn_container,
        });

        search_container |= CatchEvent([&](Event event) { return false; });

        Component search_window = Renderer(search_container, [&] {
            return window(text(" Search "), search_container->Render());
        }) | CatchEvent([&](Event event){
            return search_container->OnEvent(event);
        });
        
        search_container |= border;
        /* ======= END SEARCH COMPONENTS ======== */

        /* ======= BEGIN SETTINGS ========== */
        
        int selected_datatype_index = 2, prev_datatype_index = selected_datatype_index;
        std::vector<std::string> dropdown_opts;
        for (const auto& [datatype_mode, str] : datatype_mode_string_map)
            dropdown_opts.emplace_back(str);

        Component datatype_dropdown = Dropdown(dropdown_opts, &selected_datatype_index);

        Component datatype_dropdown_labeled = Renderer(datatype_dropdown, [&] {
            if (selected_datatype_index != prev_datatype_index) {
                datatype_mode = datatype_mode_string_map[selected_datatype_index].first;
                prev_datatype_index = selected_datatype_index;
                search_str.clear();
            }
            return hbox ({
                    text(" Datatype: ") | size(WIDTH, EQUAL, 12), 
                    datatype_dropdown->Render(),
            });
        });

        Component settings_container = Container::Vertical({
            datatype_dropdown_labeled,
        });

        Component settings_window = Renderer(settings_container, [&] {
            return window(text(" Settings "), settings_container->Render());
        });

        /* ======= END SETTINGS ========== */

        const int min_width = 80, min_height = 15;

        Component master_container = Container::Horizontal({
            search_window | xflex,
            settings_window | xflex,
        });

        Component final_ui = Renderer([&] {
            if (screen.dimx() >= min_width && screen.dimy() >= min_height) {
                return master_container->Render();
            }
            return vbox({
                text("Warning: window is too small!"),
                text("Please resize to at least " + std::to_string(min_width) + "x" + std::to_string(min_height)),
                text(std::format("Current window size: {:d}x{:d}", screen.dimx(), screen.dimy()))
            }) | center | border | color(Color::Red);
        });

        screen.Loop(final_ui);
    }
}
