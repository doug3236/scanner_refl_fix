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

#ifndef SCANNERREFLFIX_H
#define SCANNERREFLFIX_H

#include <string>
#include <chrono>

using std::string;

struct Options {
    string profile_name{ "" };                  // optional file name of profile to attach to corrected image
    bool batch_mode = false;                    // Batch mode, auto rename with _f append to each tif file name
    string batch_file = "";                     // Batch file mode, read commands from file
    int force_ouput_bits = 0;                   // Force 16 bit output file. 8 bit input files default to 8 bit output files
    bool adjust_to_detected_white = false;      // Scales output values so that the largest .01% of pixels are maxed (255)
    bool save_intermediate_files = false;       // Saves various intermediate files for debugging
    float gain_restore_scale = -1.0f;           // Increase RGB values to optimize performance against uncorrected profiles
    bool make_rgblab_cgats = false;             // Make rgblab cgats file for icc profile creation
    bool landscape = false;                     // tif targets are in landscape, default is profile
    bool simulate_reflected_light = false;      // generate an image estimate of scanner's re-reflected light addition.
    float edge_reflectance = .85f;              // average reflected light of area outside of scan crop (if black: .01)
    bool reflection_stats =false;               // read in standard scatter 35x29 chart and print metrics
    bool print_line_and_time = false;           // print line number and time since start for each major phase of process
    bool correct_image_in_aRGB = false;         // correct image from scanner that has been converted to Adobe RGB
    string calibration_file = "scanner_cal.txt";// Default Scanner reflection calibration text file
    bool scanner_cal=false;                     // Scanner reflection calibration tif file?
    string executable_directory = "";           // contains directory where executable is located
    vector<float> Y;                            // optional Y values for determining gamma
};


// Used to time functions to optimize multi-threading, etc.
struct Timer {
    int count = 0;
    std::chrono::system_clock::time_point snapTime, tmp;
    double cumTime = 0;
    Timer() { reset(); }
    void reset() { count = 0; cumTime = 0; start(); }
    void start() { snapTime = std::chrono::system_clock::now(); }
    double stop() { cumTime += std::chrono::duration<double>((tmp = std::chrono::system_clock::now()) - snapTime).count(); count++; snapTime = tmp; return cumTime; }
};

#endif

void calibrate_scanner(std::vector<std::string>& cmdLine, Options& options);
