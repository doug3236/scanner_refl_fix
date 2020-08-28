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

#ifndef REFL_HELPERS_H
#define REFL_HELPERS_H

#include <vector>
#include <iostream>
#include <array>
#include <fstream>
#include <algorithm>
#include <future>
#include "ArgumentParse.h"
#include "tiffresults.h"
#include "interpolate.h"
#include "ScannerReflFix.h"
#include "Calibration.h"
#include "PatchChart.h"
#include "validation.h"
#include "cgats.h"
#include "statistics.h"

extern struct Options options;

void process_image(const std::string &image_in_raw, std::string image_out, Timer& timer);
void process_args(std::vector<std::string>& args, Options& options);
std::pair<std::vector<std::string>, Options> process_a_command_line(std::vector<std::string> args);
void message_and_exit(std::string message);
void make_rgblab(std::vector<std::string> cmdLine);
void calibrate_scanner(std::vector<std::string>& cmdLine, Options& options);
void get_scatter_stats(std::string filename);

#endif
