/***************************************************************************
 * ROM Properties Page shell extension. (librpfile)                        *
 * FileSystem.hpp: File system functions.                                  *
 *                                                                         *
 * Copyright (c) 2016-2021 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#ifndef __ROMPROPERTIES_LIBRPFILE_FILESYSTEM_HPP__
#define __ROMPROPERTIES_LIBRPFILE_FILESYSTEM_HPP__

#include <stdint.h>

// access() macros.
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

// Windows doesn't define X_OK, W_OK, or R_OK.
// NOTE: F_OK doesn't work properly on Windows.
#ifndef F_OK
#define F_OK 0
#endif
#ifndef X_OK
#define X_OK 1
#endif
#ifndef W_OK
#define W_OK 2
#endif
#ifndef R_OK
#define R_OK 4
#endif

// Directory separator characters.
#include "tcharx.h"

// C++ includes.
#include <string>

namespace LibRpFile { namespace FileSystem {

/**
 * Recursively mkdir() subdirectories.
 *
 * The last element in the path will be ignored, so if
 * the entire pathname is a directory, a trailing slash
 * must be included.
 *
 * NOTE: Only native separators ('\\' on Windows, '/' on everything else)
 * are supported by this function.
 *
 * @param path Path to recursively mkdir. (last component is ignored)
 * @return 0 on success; non-zero on error.
 */
int rmkdir(const std::string &path);

/**
 * Does a file exist?
 * @param pathname Pathname.
 * @param mode Mode.
 * @return 0 if the file exists with the specified mode; non-zero if not.
 */
int access(const std::string &pathname, int mode);

/**
 * Get a file's size.
 * @param filename Filename.
 * @return Size on success; -1 on error.
 */
off64_t filesize(const std::string &filename);

/**
 * Get the user's cache directory.
 * This is usually one of the following:
 * - Windows XP: %APPDATA%\Local Settings\rom-properties\cache
 * - Windows Vista: %LOCALAPPDATA%\rom-properties\cache
 * - Linux: ~/.cache/rom-properties
 *
 * @return User's rom-properties cache directory, or empty string on error.
 */
const std::string &getCacheDirectory(void);

/**
 * Get the user's rom-properties configuration directory.
 * This is usually one of the following:
 * - Windows: %APPDATA%\rom-properties
 * - Linux: ~/.config/rom-properties
 *
 * @return User's rom-properties configuration directory, or empty string on error.
 */
const std::string &getConfigDirectory(void);

/**
 * Set the modification timestamp of a file.
 * @param filename Filename.
 * @param mtime Modification time.
 * @return 0 on success; negative POSIX error code on error.
 */
int set_mtime(const std::string &filename, time_t mtime);

/**
 * Get the modification timestamp of a file.
 * @param filename Filename.
 * @param pMtime Buffer for the modification timestamp.
 * @return 0 on success; negative POSIX error code on error.
 */
int get_mtime(const std::string &filename, time_t *pMtime);

/**
 * Delete a file.
 * @param filename Filename.
 * @return 0 on success; negative POSIX error code on error.
 */
int delete_file(const char *filename);

/**
 * Delete a file.
 * @param filename Filename.
 * @return 0 on success; negative POSIX error code on error.
 */
static inline int delete_file(const std::string &filename)
{
	return delete_file(filename.c_str());
}

/**
 * Get the file extension from a filename or pathname.
 * @param filename Filename
 * @return File extension, including the leading dot (pointer to within the filename) [nullptr if no extension]
 */
const char *file_ext(const std::string &filename);

#ifdef _WIN32
/**
 * Get the file extension from a filename or pathname. (wchar_t version)
 * @param filename Filename
 * @return File extension, including the leading dot (pointer to within the filename) [nullptr if no extension]
 */
const wchar_t *file_ext(const std::wstring& filename);
#endif /* _WIN32 */

/**
 * Replace the file extension from a filename.
 * @param filename	[in] Filename.
 * @param ext		[in] New extension.
 * @return Filename, with replaced extension.
 */
std::string replace_ext(const char *filename, const char *ext);

/**
 * Check if the specified file is a symbolic link.
 * @return True if the file is a symbolic link; false if not.
 */
bool is_symlink(const char *filename);

/**
 * Resolve a symbolic link.
 *
 * If the specified filename is not a symbolic link,
 * the filename will be returned as-is.
 *
 * @param filename Filename of symbolic link.
 * @return Resolved symbolic link, or empty string on error.
 */
std::string resolve_symlink(const char *filename);

/**
 * Is a file located on a "bad" file system?
 *
 * We don't want to check files on e.g. procfs,
 * or on network file systems if the option is disabled.
 *
 * @param filename Filename.
 * @param netFS If true, allow network file systems.
 *
 * @return True if this file is on a "bad" file system; false if not.
 */
bool isOnBadFS(const char *filename, bool netFS = false);

/**
 * Get a file's size and time.
 * @param filename	[in] Filename.
 * @param pFileSize	[out] File size.
 * @param pMtime	[out] Modification time.
 * @return 0 on success; negative POSIX error code on error.
 */
int get_file_size_and_mtime(const std::string &filename, off64_t *pFileSize, time_t *pMtime);

} }

#endif /* __ROMPROPERTIES_LIBRPFILE_FILESYSTEM_HPP__ */
