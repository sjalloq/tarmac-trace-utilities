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

#ifndef TARMAC_BROWSER_DISASSEMBLY_HH
#define TARMAC_BROWSER_DISASSEMBLY_HH

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

struct DisasmLine {
    enum Type { Instruction, Source, FunctionHeader, Blank };
    Type type;
    std::string text;
    uint64_t address;       // valid for Instruction and FunctionHeader
    std::string func_name;  // valid for FunctionHeader
};

class DisassemblyFile {
    std::vector<DisasmLine> lines;
    // sorted (addr -> line idx) for binary search lookup
    std::vector<std::pair<uint64_t, size_t>> addr_index;
    uint64_t load_offset;

  public:
    DisassemblyFile(const std::string &filename, uint64_t load_offset);
    // Returns SIZE_MAX if not found
    size_t find_line_for_pc(uint64_t trace_pc) const;
    const DisasmLine &line_at(size_t idx) const;
    size_t total_lines() const;
};

#endif // TARMAC_BROWSER_DISASSEMBLY_HH
