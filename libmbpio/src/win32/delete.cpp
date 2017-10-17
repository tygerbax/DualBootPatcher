/*
 * Copyright (C) 2015-2017  Andrew Gunnerson <andrewgunnerson@gmail.com>
 *
 * This file is part of DualBootPatcher
 *
 * DualBootPatcher is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * DualBootPatcher is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with DualBootPatcher.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "mbpio/win32/delete.h"

#include <windows.h>

#include <memory>
#include <type_traits>

#include "mbcommon/error/error.h"
#include "mbcommon/error/type/ec_error.h"
#include "mbcommon/locale.h"

namespace mb
{
namespace io
{
namespace win32
{

using ScopedFindHandle = std::unique_ptr<std::remove_pointer_t<HANDLE>, decltype(FindClose) *>;

static Expected<void> _win32_recursive_delete(const std::wstring &path)
{
    std::wstring mask(path);
    mask += L"\\*";

    WIN32_FIND_DATAW find_data;

    // First, delete the contents of the directory, recursively for subdirectories
    HANDLE _search_handle = FindFirstFileExW(
        mask.c_str(),           // lpFileName
        FindExInfoBasic,        // fInfoLevelId
        &find_data,             // lpFindFileData
        FindExSearchNameMatch,  // fSearchOp
        nullptr,                // lpSearchFilter
        0                       // dwAdditionalFlags
    );
    if (_search_handle == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        if (error != ERROR_FILE_NOT_FOUND) {
            return make_error<ECError>(ECError::from_win32_error(error));
        }
    } else {
        ScopedFindHandle search_handle(_search_handle, &FindClose);

        while (true) {
            if (wcscmp(find_data.cFileName, L".") != 0
                    && wcscmp(find_data.cFileName, L"..") != 0) {
                bool is_directory = (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                        || (find_data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT);
                std::wstring child_path(path);
                child_path += L'\\';
                child_path += find_data.cFileName;

                if (is_directory) {
                    auto ret = _win32_recursive_delete(child_path);
                    if (!ret) {
                        return ret.take_error();
                    }
                } else {
                    if (!DeleteFileW(child_path.c_str())) {
                        return make_error<ECError>(
                                ECError::from_win32_error(GetLastError()));
                    }
                }
            }

            // Advance to the next file in the directory
            if (!FindNextFileW(search_handle.get(), &find_data)) {
                DWORD error = GetLastError();
                if (error != ERROR_NO_MORE_FILES) {
                    return make_error<ECError>(
                            ECError::from_win32_error(error));
                }
                break;
            }
        }
    }

    if (!RemoveDirectoryW(path.c_str())) {
        return make_error<ECError>(ECError::from_win32_error(GetLastError()));
    }

    return {};
}

Expected<void> delete_recursively(const std::string &path)
{
    auto w_path = mb::utf8_to_wcs(path);
    if (!w_path) {
        return w_path.take_error();
    }

    return _win32_recursive_delete(*w_path);
}

}
}
}
