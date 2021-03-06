// Copyright 2017 The Bazel Authors. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// For rand_s function, https://msdn.microsoft.com/en-us/library/sxtz2fa8.aspx
#define _CRT_RAND_S
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <algorithm>
#include <sstream>
#include <string>

#include "src/main/cpp/util/file_platform.h"
#include "src/tools/launcher/util/launcher_util.h"

namespace bazel {
namespace launcher {

using std::ostringstream;
using std::string;
using std::wstring;
using std::stringstream;

string GetLastErrorString() {
  DWORD last_error = GetLastError();
  if (last_error == 0) {
    return string();
  }

  char* message_buffer;
  size_t size = FormatMessageA(
      FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
          FORMAT_MESSAGE_IGNORE_INSERTS,
      NULL, last_error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
      (LPSTR)&message_buffer, 0, NULL);

  stringstream result;
  result << "(error: " << last_error << "): " << message_buffer;
  LocalFree(message_buffer);
  return result.str();
}

void die(const char* format, ...) {
  va_list ap;
  va_start(ap, format);
  fputs("LAUNCHER ERROR: ", stderr);
  vfprintf(stderr, format, ap);
  va_end(ap);
  fputc('\n', stderr);
  exit(1);
}

void PrintError(const char* format, ...) {
  va_list ap;
  va_start(ap, format);
  fputs("LAUNCHER ERROR: ", stderr);
  vfprintf(stderr, format, ap);
  va_end(ap);
  fputc('\n', stderr);
}

wstring AsAbsoluteWindowsPath(const char* path) {
  wstring wpath;
  if (!blaze_util::AsAbsoluteWindowsPath(path, &wpath)) {
    die("Couldn't convert %s to absoulte Windows path.", path);
  }
  return wpath;
}

bool DoesFilePathExist(const char* path) {
  DWORD dwAttrib = GetFileAttributesW(AsAbsoluteWindowsPath(path).c_str());

  return (dwAttrib != INVALID_FILE_ATTRIBUTES &&
          !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

bool DoesDirectoryPathExist(const char* path) {
  DWORD dwAttrib = GetFileAttributesW(AsAbsoluteWindowsPath(path).c_str());

  return (dwAttrib != INVALID_FILE_ATTRIBUTES &&
          (dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

bool DeleteFileByPath(const char* path) {
  return DeleteFileW(AsAbsoluteWindowsPath(path).c_str());
}

string GetBinaryPathWithoutExtension(const string& binary) {
  if (binary.find(".exe", binary.size() - 4) != string::npos) {
    return binary.substr(0, binary.length() - 4);
  }
  return binary;
}

string GetBinaryPathWithExtension(const string& binary) {
  return GetBinaryPathWithoutExtension(binary) + ".exe";
}

string GetEscapedArgument(const string& argument, bool escape_backslash) {
  ostringstream escaped_arg;
  bool has_space = argument.find_first_of(' ') != string::npos;

  if (has_space) {
    escaped_arg << '\"';
  }

  string::const_iterator it = argument.begin();
  while (it != argument.end()) {
    char ch = *it++;
    switch (ch) {
      case '"':
        // Escape double quotes
        escaped_arg << "\\\"";
        break;

      case '\\':
        // Escape back slashes if escape_backslash is true
        escaped_arg << (escape_backslash ? "\\\\" : "\\");
        break;

      default:
        escaped_arg << ch;
    }
  }

  if (has_space) {
    escaped_arg << '\"';
  }
  return escaped_arg.str();
}

// An environment variable has a maximum size limit of 32,767 characters
// https://msdn.microsoft.com/en-us/library/ms683188.aspx
static const int BUFFER_SIZE = 32767;

bool GetEnv(const string& env_name, string* value) {
  char buffer[BUFFER_SIZE];
  if (!GetEnvironmentVariableA(env_name.c_str(), buffer, BUFFER_SIZE)) {
    return false;
  }
  *value = buffer;
  return true;
}

bool SetEnv(const string& env_name, const string& value) {
  return SetEnvironmentVariableA(env_name.c_str(), value.c_str());
}

string GetRandomStr(size_t len) {
  static const char alphabet[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
  string rand_str;
  rand_str.reserve(len);
  unsigned int x;
  for (size_t i = 0; i < len; i++) {
    rand_s(&x);
    rand_str += alphabet[x % strlen(alphabet)];
  }
  return rand_str;
}

bool NormalizePath(const string& path, string* result) {
  if (!blaze_util::AsWindowsPath(path, result)) {
    PrintError("Failed to normalize %s", path.c_str());
    return false;
  }
  std::transform(result->begin(), result->end(), result->begin(), ::tolower);
  return true;
}

bool RelativeTo(const string& path, const string& base, string* result) {
  if (blaze_util::IsAbsolute(path) != blaze_util::IsAbsolute(base)) {
    PrintError(
        "Cannot calculate relative path from an absolute and a non-absolute"
        " path.\npath = %s\nbase = %s",
        path.c_str(), base.c_str());
    return false;
  }

  if (blaze_util::IsAbsolute(path) && blaze_util::IsAbsolute(base) &&
      path[0] != base[0]) {
    PrintError(
        "Cannot calculate relative path from absolute path under different "
        "drives."
        "\npath = %s\nbase = %s",
        path.c_str(), base.c_str());
    return false;
  }

  // Record the back slash position after the last matched path fragment
  int pos = 0;
  int back_slash_pos = -1;
  while (path[pos] == base[pos] && base[pos] != '\0') {
    if (path[pos] == '\\') {
      back_slash_pos = pos;
    }
    pos++;
  }

  if (base[pos] == '\0' && path[pos] == '\0') {
    // base == path in this case
    result->assign("");
    return true;
  }

  if ((base[pos] == '\0' && path[pos] == '\\') ||
      (base[pos] == '\\' && path[pos] == '\0')) {
    // In this case, one of the paths is the parent of another one.
    // We should move back_slash_pos to the end of the shorter path.
    // eg. path = c:\foo\bar, base = c:\foo => back_slash_pos = 6
    //  or path = c:\foo, base = c:\foo\bar => back_slash_pos = 6
    back_slash_pos = pos;
  }

  ostringstream buffer;

  // Create the ..\\ prefix
  // eg. path = C:\foo\bar1, base = C:\foo\bar2, we need ..\ prefix
  // In case "base" is a parent of "path", we set back_slash_pos to the end
  // of "base", so we need no prefix when back_slash_pos + 1 > base.length().
  // back_slash_pos + 1 == base.length() is not possible because the last
  // character of a normalized path won't be back slash.
  if (back_slash_pos + 1 < base.length()) {
    buffer << "..\\";
  }
  for (int i = back_slash_pos + 1; i < base.length(); i++) {
    if (base[i] == '\\') {
      buffer << "..\\";
    }
  }

  // Add the result of not matched path fragments into result
  // eg. path = C:\foo\bar1, base = C:\foo\bar2, adding `bar1`
  // In case "path" is a parent of "base", we set back_slash_pos to the end
  // of "path", so we need no suffix when back_slash_pos == path.length().
  if (back_slash_pos != path.length()) {
    buffer << path.substr(back_slash_pos + 1);
  }

  result->assign(buffer.str());
  return true;
}

}  // namespace launcher
}  // namespace bazel
