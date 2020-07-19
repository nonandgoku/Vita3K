// Vita3K emulator project
// Copyright (C) 2020 Vita3K team
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

#pragma once

#include <gui/state.h>

#include <cstdint>
#include <string>
#include <vector>

struct DisplayState;
struct HostState;
struct SDL_Window;
struct NpTrophyUnlockCallbackData;

namespace gui {

enum GenericDialogState {
    UNK_STATE,
    CONFIRM_STATE,
    CANCEL_STATE
};

void delete_app(GuiState &gui, HostState &host);
std::vector<App>::iterator get_app_index(GuiState &gui, const std::string &title_id);
void get_modules_list(GuiState &gui, HostState &host);
void get_themes_list(GuiState &gui, HostState &host);
void get_trophy_np_com_id_list(GuiState &gui, HostState &host);
void get_user_apps_title(GuiState &gui, HostState &host);
void init(GuiState &gui, HostState &host);
void init_app_background(GuiState &gui, HostState &host);
void init_apps_icon(GuiState &gui, HostState &host, const std::vector<gui::App> &apps_list);
void init_live_area(GuiState &gui, HostState &host);
bool init_manual(GuiState &gui, HostState &host);
bool init_theme(GuiState &gui, HostState &host, const std::string &content_id);
void init_theme_apps_icon(GuiState &gui, HostState &host, const std::string &content_id);
void init_theme_start_background(GuiState &gui, HostState &host, const std::string &content_id);
bool init_user_start_background(GuiState &gui, const std::string &image_path);
void pre_load_app(GuiState &gui, HostState &host);
void pre_run_app(GuiState &gui, HostState &host);
bool refresh_app_list(GuiState &gui, HostState &host);

void draw_begin(GuiState &gui, HostState &host);
void draw_end(GuiState &host, SDL_Window *window);
void draw_live_area(GuiState &gui, HostState &host);
void draw_ui(GuiState &gui, HostState &host);

void draw_app_context_menu(GuiState &gui, HostState &host);
void draw_common_dialog(GuiState &gui, HostState &host);
void draw_information_bar(GuiState &gui);
void draw_reinstall_dialog(GenericDialogState *status);
void draw_trophies_unlocked(GuiState &gui, HostState &host);
void draw_perf_overlay(GuiState &gui, HostState &host);

ImTextureID load_image(GuiState &gui, const char *data, const std::uint32_t size);

} // namespace gui

// Extensions to ImGui
namespace ImGui {

bool vector_getter(void *vec, int idx, const char **out_text);
}
