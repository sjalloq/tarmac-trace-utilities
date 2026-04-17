/*
 * Copyright 2024 Arm Limited. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * This file is part of Tarmac Trace Utilities
 */

#include "disassembly.hh"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <stdexcept>

static bool is_hex_digit(char c)
{
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
           (c >= 'A' && c <= 'F');
}

static bool is_blank_line(const std::string &s)
{
    for (char c : s)
        if (!isspace((unsigned char)c))
            return false;
    return true;
}

// Try to parse an instruction line: optional leading whitespace, then hex
// digits followed by ':'.  Examples:
//   "   80001a4:	e92d 4ff0 	stmdb	sp!, ..."
//   "20000000:    480c          ldr    r0, [pc, #48]"
static bool parse_instruction_line(const std::string &line, uint64_t &addr)
{
    size_t i = 0;
    // Leading whitespace is optional
    while (i < line.size() && isspace((unsigned char)line[i]))
        i++;
    // Now expect hex digits
    size_t hex_start = i;
    while (i < line.size() && is_hex_digit(line[i]))
        i++;
    if (i == hex_start || i >= line.size() || line[i] != ':')
        return false;
    // Parse the address
    char *end = nullptr;
    addr = strtoull(line.c_str() + hex_start, &end, 16);
    return end == line.c_str() + i;
}

// Try to parse a function header line: starts with hex digits at column 0,
// followed by " <name>:".  Example: "080001a4 <main>:"
static bool parse_function_header(const std::string &line, uint64_t &addr,
                                  std::string &name)
{
    size_t i = 0;
    if (i >= line.size() || !is_hex_digit(line[i]))
        return false;
    while (i < line.size() && is_hex_digit(line[i]))
        i++;
    if (i == 0)
        return false;
    // Expect " <"
    if (i >= line.size() || line[i] != ' ')
        return false;
    i++;
    if (i >= line.size() || line[i] != '<')
        return false;
    i++;
    size_t name_start = i;
    while (i < line.size() && line[i] != '>')
        i++;
    if (i >= line.size())
        return false;
    size_t name_end = i;
    i++; // skip '>'
    if (i >= line.size() || line[i] != ':')
        return false;

    char *end = nullptr;
    addr = strtoull(line.c_str(), &end, 16);
    name = line.substr(name_start, name_end - name_start);
    return true;
}

DisassemblyFile::DisassemblyFile(const std::string &filename,
                                 uint64_t load_offset_)
    : load_offset(load_offset_)
{
    std::ifstream f(filename);
    if (!f.is_open())
        throw std::runtime_error("Cannot open disassembly file: " + filename);

    std::string line;
    while (std::getline(f, line)) {
        DisasmLine dl;
        dl.address = 0;

        uint64_t addr;
        std::string func_name;

        if (is_blank_line(line)) {
            dl.type = DisasmLine::Blank;
        } else if (parse_function_header(line, addr, func_name)) {
            dl.type = DisasmLine::FunctionHeader;
            dl.address = addr;
            dl.func_name = func_name;
        } else if (parse_instruction_line(line, addr)) {
            dl.type = DisasmLine::Instruction;
            dl.address = addr;
            addr_index.push_back({addr, lines.size()});
        } else {
            dl.type = DisasmLine::Source;
        }

        dl.text = line;
        lines.push_back(dl);
    }

    // Sort addr_index by address for binary search
    std::sort(addr_index.begin(), addr_index.end());
}

size_t DisassemblyFile::find_line_for_pc(uint64_t trace_pc) const
{
    // Convert trace PC (loaded address) to image-space address
    uint64_t image_addr = trace_pc - load_offset;

    // Use lower_bound to find the nearest address at or before target
    auto it = std::upper_bound(
        addr_index.begin(), addr_index.end(), image_addr,
        [](uint64_t val, const std::pair<uint64_t, size_t> &entry) {
            return val < entry.first;
        });

    if (it == addr_index.begin())
        return SIZE_MAX;

    --it;

    if (it->first == image_addr)
        return it->second;

    // Thumb PC values carry bit 0 set to indicate Thumb mode;
    // objdump emits the even (instruction) address.
    if ((image_addr & 1) && it->first == (image_addr & ~1ULL))
        return it->second;

    return SIZE_MAX;
}

const DisasmLine &DisassemblyFile::line_at(size_t idx) const
{
    return lines[idx];
}

size_t DisassemblyFile::total_lines() const { return lines.size(); }
