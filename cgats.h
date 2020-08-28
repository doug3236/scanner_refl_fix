/*
Copyright (c) <2020> <doug gray>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#ifndef CGATS_H
#define CGATS_H

#include <vector>
#include <utility>
#include <string>
#include <array>
#include <fstream>
#include <iostream>
#include <tuple>
#include <cassert>

#include "statistics.h"

namespace cgats_utilities {
    // gathers CGATs file info
    struct Cgats_data {
        std::string filename;
        std::vector<std::vector<std::string>> lines;   // tokenized lines in CGATs file
        std::vector<std::string> fields;          // tokenized list of fields
        std::vector<std::vector<std::string>>::iterator data_ptr; // pointer to start of value lines
        int num_of_fields;  // num_of_fields==fields.size()==data_ptr[0].size()
        int num_of_sets;    // data_ptr[0] to data_ptr[num_of_sets-1]
        ptrdiff_t rgb_loc;  // location of RGB_R in fields
        ptrdiff_t lab_loc;  // location of LAB_L in fields
    };
    Cgats_data populate_cgats(const std::string& filename);
    std::vector<std::string> parse(const std::string& s);
    std::vector<std::vector<std::string>> tokenize_file(const std::string& filename);

    using V3= std::array<float, 3>;  // typically used to hold RGB, LAB or XYZ values
    using V6= std::array<float, 6>;  // typically used to hold RGBLAB value sets

    std::vector<V6> read_cgats_rgblab(const std::string& filename, bool include_lab = true);
    std::vector<V3> read_cgats_rgb(const std::string& filename);

    bool write_cgats_rgb(const std::vector<V3>& rgb, const std::string& filename);  // write rgb patch file
    bool write_cgats_lab(const std::vector<V3>& lab, const std::string& filename);  // 288 lab file for scanner reference creation
    bool write_cgats_rgblab(const std::vector<V6>& rgblab, const std::string& filename, std::string descriptor = "RGBLAB");
    std::pair<std::vector<V3>, std::vector<V3>> separate_rgb_lab(const std::vector<V6>& rgblab);
    std::vector<V6> combine_rgb_lab(const std::vector<V3>& rgb, const std::vector<V3>& lab);

    // predicate for sort by RGB values
    bool less_than(const V6& arg1, const V6& arg2);
    // Overrides operator== for array<> to look at only the first 3 values
    bool operator==(const V6& arg1, const V6& arg2);
    
    // used to collect statistics when combining the same RGB patch values
    struct DuplicateStats {
        V6 rgb_lab;
        Statistics lab[3];
    };

    // sort and remove duplicates from patch sets
    std::vector<DuplicateStats> remove_duplicates(std::vector<V6> vals);
}
#endif
