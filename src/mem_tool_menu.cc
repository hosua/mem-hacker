#include "mem_tool_menu.hh"
#include "mem_tool.hh"

#include <ftxui/dom/elements.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>

#include <string>
#include <vector>
#include <iostream>

static int selected_result;
static std::vector<std::string> results = {
};
static DatatypeMode datatype_mode = DTM_U32;

namespace MemToolMenu {
    using namespace ftxui;
    
    void run(int pid) {
        ScreenInteractive screen = ScreenInteractive::Fullscreen();
        
        /* ======= BEGIN SEARCH COMPONENTS ======= */

        auto results_menu = Menu(&results, &selected_result);
        Component results_list = Container::Vertical({
            Renderer(results_menu, [&] {
                return results_menu->Render() | vscroll_indicator | frame | border;
            })
        });

        Component search_button = Button({ 
            .label = "Search",
            .on_click = [&]() {
                // search for memory values 
            },
        });

        std::string search_str;

        Component search_input = Input({
            .content = &search_str,
            .placeholder = "enter a value to search...",
            .transform = [](InputState state) {
                return state.element |= bgcolor(Color::GrayDark);
            },
            .multiline = false,
            .on_enter = [&] {
                
                // search for memory values 
                search_str.clear();
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

        Component search_bar = Container::Horizontal({
            search_input, search_button
        });

        Component search_container = Container::Vertical({
            results_list,
            search_bar,
        }) | size(WIDTH, GREATER_THAN, 100);
        
        search_container |= border;
        /* ======= END SEARCH COMPONENTS ======== */

        /* ======= BEGIN SETTINGS ========== */
        
        int selected_datatype_index = 0, prev_datatype_index = 0;
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

        Component settings_titled = Renderer(settings_container, [&] {
            return window(text(" Settings "), settings_container->Render()) | xflex;
        });

        /* ======= END SETTINGS ========== */

        Component master_container = Container::Horizontal({
            search_container,
            settings_titled,
        });

        screen.Loop(master_container);
    }
}
