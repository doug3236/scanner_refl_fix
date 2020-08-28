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
#include "Refl_helpers.h"


using std::vector;
using std::string;
using std::cout;
using std::cin;
using std::endl;

Options options;
#include <windows.h>

int main(int argc, char const** argv)
{
    Timer timer;
    if (argc==1)
        message_and_exit("");
    vector<string> cmdArgs = vectorize_commands(argc, argv);
    cmdArgs.erase(cmdArgs.begin());

    // process options, all options must be valid and at least one file argument remaining
    try
    {
        vector<vector<string>> argList{cmdArgs};
        process_args(cmdArgs, options);
        validate(options.force_output_bits == 0 || options.force_output_bits == 8 || options.force_output_bits == 16, "-F n:   n must be either 8 or 16");

        // use batch file list of commands if -b "batchfile.txt" selected
        if (options.batch_file != "")
        {
            argList.erase(argList.begin());
            validate(cmdArgs.size()==0, "-b batchfile must not have any other arguments");
            argList = cgats_utilities::tokenize_file(options.batch_file);
        }
        for (auto cmdLine : argList)
        {
            // Skip comment lines (start with %) and blank lines
            if (cmdLine.size()==0 || cmdLine[0].length() == 0 || cmdLine[0].substr(0,1)=="%")
                continue;
            // load a new Options with defaults then set exec directory
            process_args(cmdLine, options);
            validate(cmdLine.size() != 0 || options.batch_file != "", "command line error");
            validate(options.force_output_bits == 0 || options.force_output_bits == 8 || options.force_output_bits == 16, "-F n:   n must be either 8 or 16");

            // UTILITY: Process scanned target and optionally CGATs measurement
            if (options.make_rgblab_cgats)
                make_rgblab(cmdLine);

            // UTILITY Generate scanner calibration file from scanned calibration image
            // optional arguments: "Y" values of small patches left to right from spectrophotometer
            else if (options.scanner_cal)
                calibrate_scanner(cmdLine, options);

            // UTILITY Analyzing Reflection Chart RGB Variations
            else if (options.reflection_stats)
                get_scatter_stats(cmdLine[0]);

            else if (!options.batch_mode)
            {
                // remove (or add) reflections from first file and save to second file
                validate(cmdLine.size() == 2, "Arguments must include input tif file and output tif file");
                process_image(cmdLine[0], cmdLine[1], timer);
            }
            else
            {
                // remove (or add) reflections from 1, 3 or more files, rename with "_f" appended
                validate(cmdLine.size() >= 1, "Arguments must include 1 or more input tif files");
                for (int i = 0; i < int(cmdLine.size()); i++)
                    process_image(cmdLine[i], "", timer);
            }
        }
    }
    catch (const std::exception &e)
    {
        cout << e.what() << endl;
        exit(-1);
    }
    catch (const char *e)
    {
        cout << e << std::endl;
        exit(-1);
    }
    catch (...) {
        cout << "unknown exception" << std::endl;
        exit(-1);
    }
    cout << "Execution Time: " << timer.stop() << endl;
}

