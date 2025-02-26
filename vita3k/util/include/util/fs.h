// Vita3K emulator project
// Copyright (C) 2023 Vita3K team
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

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

namespace fs = boost::filesystem;

class Root {
    fs::path base_path;
    fs::path pref_path;
    fs::path log_path;
    fs::path config_path;
    fs::path shared_path;
    fs::path cache_path;

public:
    void set_base_path(const fs::path &p) {
        base_path = p;
    }

    fs::path get_base_path() const {
        return base_path;
    }

    std::string get_base_path_string() const {
        return base_path.generic_path().string();
    }

    void set_pref_path(const fs::path &p) {
        pref_path = p;
    }

    fs::path get_pref_path() const {
        return pref_path;
    }

    std::string get_pref_path_string() const {
        return pref_path.generic_path().string();
    }

    void set_log_path(const fs::path &p) {
        log_path = p;
    }

    fs::path get_log_path() const {
        return log_path;
    }

    std::string get_log_path_string() const {
        return log_path.generic_path().string();
    }

    void set_config_path(const fs::path &p) {
        config_path = p;
    }

    fs::path get_config_path() const {
        return config_path;
    }

    std::string get_config_path_string() const {
        return config_path.generic_path().string();
    }

    void set_shared_path(const fs::path &p) {
        shared_path = p;
    }

    fs::path get_shared_path() const {
        return shared_path;
    }

    std::string get_shared_path_string() const {
        return shared_path.generic_path().string();
    }

    void set_cache_path(const fs::path &p) {
        cache_path = p;
    }

    fs::path get_cache_path() const {
        return cache_path;
    }

    std::string get_cache_path_string() const {
        return cache_path.generic_path().string();
    }
}; // class root

namespace fs_utils {

/**
 * \brief  Construct a file name (optionally with an extension) to be placed in a Vita3K directory.
 * \param  base_path   The main output path for the file.
 * \param  folder_path The sub-directory/sub-directories to output to.
 * \param  file_name   The name of the file.
 * \param  extension   The extension of the file (optional)
 * \return A complete Boost.Filesystem file path normalized.
 */
inline fs::path construct_file_name(const fs::path &base_path, const fs::path &folder_path, const fs::path &file_name, const fs::path &extension = "") {
    fs::path full_file_path{ base_path / folder_path / file_name };
    if (!extension.empty())
        full_file_path.replace_extension(extension);

    return full_file_path.generic_path();
}

} // namespace fs_utils
