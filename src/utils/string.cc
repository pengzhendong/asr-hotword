// Copyright (c) 2023 Zhendong Peng (pzd17@tsinghua.org.cn)
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "utils/string.h"

#include "glog/logging.h"

namespace asr {

void SplitUTF8StringToChars(const std::string& str,
                            std::vector<std::string>* chars) {
  chars->clear();
  int bytes = 1;
  for (size_t i = 0; i < str.length(); i += bytes) {
    CHECK_LE(str[i] & 0xF8, 0xF0);
    if ((str[i] & 0x80) == 0x00) {
      // The first 128 characters (US-ASCII) in UTF-8 format only need one byte.
      bytes = 1;
    } else if ((str[i] & 0xE0) == 0xC0) {
      // The next 1,920 characters need two bytes to encode,
      // which covers the remainder of almost all Latin-script alphabets.
      bytes = 2;
    } else if ((str[i] & 0xF0) == 0xE0) {
      // Three bytes are needed for characters in the rest of
      // the Basic Multilingual Plane, which contains virtually all characters
      // in common use, including most Chinese, Japanese and Korean characters.
      bytes = 3;
    } else if ((str[i] & 0xF8) == 0xF0) {
      // Four bytes are needed for characters in the other planes of Unicode,
      // which include less common CJK characters, various historic scripts,
      // mathematical symbols, and emoji (pictographic symbols).
      bytes = 4;
    }
    chars->push_back(str.substr(i, bytes));
  }
}

void UTF8CharToUpper(std::vector<std::string>* chars) {
  CHECK(chars != nullptr);
  for (auto& utf8_char : *chars) {
    if (std::isalpha(utf8_char[0])) {
      utf8_char[0] = std::toupper(utf8_char[0]);
    }
  }
}

std::string Ltrim(const std::string& str) {
  size_t start = str.find_first_not_of(WHITESPACE);
  return (start == std::string::npos) ? "" : str.substr(start);
}

std::string Rtrim(const std::string& str) {
  size_t end = str.find_last_not_of(WHITESPACE);
  return (end == std::string::npos) ? "" : str.substr(0, end + 1);
}

std::string Trim(const std::string& str) { return Rtrim(Ltrim(str)); }

}  // namespace asr
