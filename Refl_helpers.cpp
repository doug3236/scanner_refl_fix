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

#include "Refl_helpers.h"
#include "algorithm"

void process_image(const string &image_in_raw, string image_out, Timer& timer)
{
    if (image_out.length() == 0)
    {
        options.save_intermediate_files = false;
        options.print_line_and_time = false;
        options.simulate_reflected_light = false;
        auto name = file_parts(image_in_raw);
        image_out = name.first + "_f.tif";
    }
    validate(file_is_tif(image_in_raw) && file_is_tif(image_out), "Only Tif files allowed");
    if (options.simulate_reflected_light)
        cout << "\nSimulating reflected light input: " << image_in_raw << "  out: " << image_out <<  "\n";
    else
        cout << "\nCorrecting reflected light input: " << image_in_raw << "  out: " << image_out <<  "\n";
    static bool firstpass=true;
    InterpolateRefl interpolate;
    interpolate.read_init_file(options.calibration_file, firstpass);
    if (firstpass)
        firstpass=false;

    if (options.gain_restore_scale < 0)  // if we haven't forced a gain restore value shift halfway to all white
        options.gain_restore_scale = .5f * interpolate.gain_adj;
    else if (options.gain_restore_scale > interpolate.gain_adj) // limit to max refl. light
        options.gain_restore_scale = interpolate.gain_adj;

    ArrayRGB image_in = TiffRead(image_in_raw.c_str(), options.correct_image_in_aRGB ? 2.2f : static_cast<float>(interpolate.gamma));

    // Get image that represents the light spread that is additive to the center's pixel location
    // top_w: number of times DPI divisible by 2, x3:  number of times DPI divisible by 3
    auto [refl_area, x2, x3] = getReflArea(image_in.dpi, interpolate);
    if (options.print_line_and_time) cout << __LINE__ << "  " << timer.stop() << endl;

    // Add 1" margin around image_in since re-reflected light model is limited to an inch
    int margins = image_in.dpi;
    ArrayRGB in_expanded{ image_in.nr + 2 * margins, image_in.nc + 2 * margins, image_in.dpi, image_in.from_16bits, image_in.gamma };
    in_expanded.fill(options.edge_reflectance, options.edge_reflectance, options.edge_reflectance);             // assume border "white" reflected 85% of light.
    in_expanded.copy(image_in, margins, margins);   // insert into expanded image with 1" margins

                                                    // for getting estimated reflected light spread
    if (options.save_intermediate_files)
    {
        cout << "Saving reflarray.tif, image of additional reflected light in gamma = 2.2" << endl;
        refl_area.gamma = 1.0f;      // write gamma for compatibility with aRGB G=1
        TiffWrite("reflArray.tif", refl_area, "");
    }
    if (options.print_line_and_time) cout << __LINE__ << "  " << timer.stop() << endl;

    // Create downsized image to calculate reflected light from
    // This does not require or need high resolution.
    ArrayRGB image_reduced = in_expanded;
    int reduction = image_in.dpi / refl_area.dpi;

    // Downsize image to create a reflected light version, use 3x downsize initially for speed
    while (x3--)
        image_reduced = downsample(image_reduced, 3);
    while (x2--)
        image_reduced = downsample(image_reduced, 2);
    if (options.print_line_and_time) cout << __LINE__ << "  " << timer.stop() << endl;


    // when logging, save downsampled file with added margin
    if (options.save_intermediate_files)
    {
        cout << "Saving imagorig.tif, reduced original file with surround in gamma=2.2" << endl;
        image_reduced.gamma = 1.0f;      // write gamma for compatibility with aRGB G=1
        TiffWrite("imageorig.tif", image_reduced, "");
    }

    // Generate reflected light image.   time consuming operation, in debug 4 min for 8x10"
    if (options.print_line_and_time) cout << __LINE__ << "  " << timer.stop() << endl;
    ArrayRGB image_correction = generate_reflected_light_estimate(image_reduced, refl_area);
    if (options.print_line_and_time) cout << __LINE__ << "  " << timer.stop() << endl;

    // save the estimated re-reflected light from the full scanned image and surround
    if (options.save_intermediate_files)
    {
        cout << "Saving refl_light.tif, image of estimated reflected light" << endl;
        image_correction.gamma = 1.0f;      // write gamma for compatibility with aRGB G=1
        TiffWrite("refl_light.tif", image_correction, "");
    }

    // Subtract re-reflected light from original
    for (int color = 0; color < 3; color++)
    {
        for (int i = 0; i < image_in.nr; i++)
        {
            for (int ii = 0; ii < image_in.nc; ii++)
            {
                float tmp;
                auto adj = bilinear(image_correction, i, ii, reduction, color) * image_in(i, ii, color);
                if (options.simulate_reflected_light)   // Special mode to simulate scanner by adding reflected light
                {
                    tmp = image_in(i, ii, color) + adj;
                    tmp /= (1.0f + options.gain_restore_scale);
                }
                else
                {
                    // gain restore  adjusts gain to offset reduction from re-reflected light subtraction
                    tmp = image_in(i, ii, color) - adj;
                    tmp = tmp * (1.0f + options.gain_restore_scale);
                }

                image_in(i, ii, color) = std::clamp(tmp, 0.f, 1.f);
            }
        }
    }
    if (options.save_intermediate_files)
    {
        cout << "Saving Corrected Image: corrected.tif" << endl;
        auto gamma = image_in.gamma;
        image_in.gamma = 1.0f;      // write gamma for compatibility with aRGB G=1
        TiffWrite("corrected.tif", image_in, "");
        image_in.gamma = gamma;
    }

    // Adjust for Relative Colorimetric w/o shift to WP (no tint change)
    // Should not be used to process scanner profiling patch scans
    if (options.adjust_to_detected_white)
    {
        float maxcolor = 0;
        for (int i = 0; i < 3; i++)
        {
            vector<float> color(image_in.v[i]);
            sort(color.begin(), color.end());
            float high = *(color.end() - (1 + color.size() / 10000));
            if (high > maxcolor) maxcolor = high;
        }
        image_in.scale(1 / maxcolor);
    }

    if (options.print_line_and_time) cout << __LINE__ << "  " << timer.stop() << endl;
    if (options.force_ouput_bits == 16)
        image_in.from_16bits = true;
    else if (options.force_ouput_bits == 8)
        image_in.from_16bits = false;

    TiffWrite(image_out.c_str(), image_in, options.profile_name);
    if (options.print_line_and_time) cout << __LINE__ << "  " << timer.stop() << endl;
}

void process_args(vector<string>& args, Options& options)
{
    procFlag("-A", args, options.correct_image_in_aRGB);
    procFlag("-B", args, options.batch_mode);
    procFlag("-b", args, options.batch_file);
    procFlag("-C", args, options.calibration_file);
    procFlag("-c", args, options.scanner_cal);
    procFlag("-F", args, options.force_ouput_bits);
    procFlag("-I", args, options.save_intermediate_files);
    procFlag("-N", args, options.gain_restore_scale);
    procFlag("-M", args, options.make_rgblab_cgats);
    procFlag("-L", args, options.landscape);
    procFlag("-P", args, options.profile_name);
    procFlag("-R", args, options.simulate_reflected_light);
    procFlag("-S", args, options.edge_reflectance);
    procFlag("-s", args, options.reflection_stats);
    procFlag("-T", args, options.print_line_and_time);
    procFlag("-W", args, options.adjust_to_detected_white);
    validate(options.force_ouput_bits == 0 || options.force_ouput_bits == 8 || options.force_ouput_bits == 16, "-F n:   n must be either 8 or 16");
}

pair<vector<string>, Options> process_a_command_line(vector<string> args)
{
    Options options;
    // process options, all options must be valid and at least one file argument remaining
    try
    {
        process_args(args, options);
        validate(args.size() != 1, "command line error");
    }
    catch (...)
    {
        throw;
    }
    return std::make_pair(args, options);
}

void message_and_exit(string message)
{
    cout << message << endl;
    cout << "Version 2.0:\n"
        "Usage: scannerreflfix                  [zero or more options] infile.tif outfile.tif\n" <<
        "  -A                                   Correct Image Already in Adobe RGB\n" <<
        "  -B tif file list                     Batch mode, auto renaming with _f\n" <<
        "  -M[L] charts... measurefile outfile  Make CGATS, chart1.tif ... CGATSmeasure.txt and save CGATS.txt\n" <<
        "  -M[L] charts... outfile              Scan patch charts and save CGATs file\n" <<
        "  -P profile                           Attach profile <profile.icc>\n" <<
        "  -S edge_refl                         ave refl outside of scanned area (0 to 1, default: .85)\n" <<
        "  -s reflection.tif                    Calculate statistics on colors with white, gray and black surrounds\n" <<
        "  -W                                   Maximize white (Like Relative Col with tint retention)\n\n" <<
        "                                       Advanced and Test options\n" <<
        "  -b batch_file                        text file with list of command lines to execute\n" <<
        "  -C calibration_file                  Use scanner reflection calibration text file.\n" <<
        "  -c scanner_cal.tif  [Y values]       Create scanner calibration file from reference scan.\n\n" <<
        "  -F 8|16                              Force 8 or 16 bit tif output]\n" <<
        "  -I                                   Save intermediate files\n" <<
        "  -N gain                              Restore gain (default half of refl matrix gain)\n" <<
        "  -R                                   Simulated scanner by adding reflected light\n" <<
        "  -T                                   Show line numbers and accumulated time.\n" <<
        "  -Z                                   Average multiple input files with No Refl. Correction.\n" <<
        "scannerreflfix.exe models and removes re-reflected light from an area\n"
        "approx 1\" around scanned RGB values for the document scanners.\n";
    exit(0);
}

void make_rgblab(std::vector<std::string> cmdLine)
{
    PatchCharts patches;
    validate(file_is_tif(cmdLine[0]), "Scanned target image must be \".tif\" file");

    std::cout << "\n\nProcessing image patch files\n";
    validate(cmdLine.size() >= 2, "Must have 1 or more scan_target_files, an optional mesurement_file, and a cgats_output_file");
    while (file_is_tif(cmdLine[0]))
    {
        patches.init(cmdLine[0], options.landscape);
        std::cout << "File:" << cmdLine[0] << " ,Rows:" << patches.charts[patches.charts.size() - 1].rows
            << ", Cols:" << patches.charts[patches.charts.size() - 1].cols << "\n";
        cmdLine.erase(cmdLine.begin());
    }
    if (options.save_intermediate_files)        // save standard deviations of RGB values if requested
    {
        auto file=file_parts(cmdLine[cmdLine.size()-1]);
        cgats_utilities::write_cgats_rgb(patches.rgb_std_255, file.first+"_std.txt");
    }

    if (cmdLine.size()==1)  // no measurement file
    {
        cgats_utilities::write_cgats_rgb(patches.rgb255, cmdLine[0]);
        std::cout << "Output RGB CGATs file:" << cmdLine[0] << ", Patches:" << patches.rgb255.size() << "\n";
    }
    else
    {
        validate(file_is_txt(cmdLine[0]), "Scanned target measurement file must be \".txt\" file");
        std::cout << "and associated CGATs measurement text file: " << cmdLine[0] << '\n';
        auto&& [rgbout_print, labout_print] = cgats_utilities::separate_rgb_lab(cgats_utilities::read_cgats_rgblab(cmdLine[0]));
        if (labout_print.size() != patches.rgb255.size())
        {
            std::cout << "tiff patch totals != CGATs meas. file size\n";
            exit(-1);
        }
        patches.rgblab_print = cgats_utilities::combine_rgb_lab(rgbout_print,labout_print);
        patches.add_lab_to_rgb(labout_print);
        validate(file_is_txt(cmdLine[1]), "Scanned rgblab output file must be \".txt\" file");
        cgats_utilities::write_cgats_rgblab(patches.rgblab, cmdLine[1]);
        std::cout << "Output RGBLAB CGATs file:" << cmdLine[1] << ", Patches:" << patches.rgblab.size() << "\n";

        // Now get scanner RGB and measured Lab for RGB 0,0,0 and 255,255,255
        auto xn1 = std::max_element(patches.rgblab_print.begin(), patches.rgblab_print.end(), [](const V6 &rgblab0, const V6 &rgblab1) {
            return rgblab0[0]+rgblab0[1]+rgblab0[2]<rgblab1[0]+rgblab1[1]+rgblab1[2];})-patches.rgblab_print.begin();
        auto xn0 =  std::min_element(patches.rgblab_print.begin(), patches.rgblab_print.end(), [](const V6 &rgblab0, const V6 &rgblab1) {
            return rgblab0[0]+rgblab0[1]+rgblab0[2]<rgblab1[0]+rgblab1[1]+rgblab1[2];})-patches.rgblab_print.begin();
        auto x0=patches.rgblab[xn0]; auto x1=patches.rgblab[xn1];
        auto f2 = [](double x) {
            char str[10];
            snprintf(str, 10, "%7.2f", x);
            std::cout << str;
        };
        auto f1 = [](double x) {
            char str[10];
            snprintf(str, 10, "%6.1f", x);
            std::cout << str;
        };
        cout << "High: Scanner RGB: "; f1(x1[0]); f1(x1[1]); f1(x1[2]);
        cout << "   Lab: "; f2(x1[3]); f2(x1[4]); f2(x1[5]); cout << '\n';
        cout << " Low: Scanner RGB: "; f1(x0[0]); f1(x0[1]); f1(x0[2]);
        cout << "   Lab: "; f2(x0[3]); f2(x0[4]); f2(x0[5]); cout << '\n';
        cout << '\n';
    }
}

void calibrate_scanner(vector<string>& cmdLine, Options& options)
{
    // optionally, CIE "Y" values (scaled to 100) can be included
    // rather than calculating them from Rel. Col. RGB values.
    // from 2 to 5 arguments with "Y" values may be included
    // corresponding to the white to gray patch and black values
    // measured with a spectrophotometer
    validate(file_is_tif(cmdLine[0]), "Scanner calibration image must be tif file");
    cout << "\nReading scanner reflection calibration file and creating \"scanner_cal.txt\"\n";
    while (cmdLine.size() > 1) {
        options.Y.push_back(std::stof(cmdLine[1]));     // store values of measured CIE "Y" reflectances
        cmdLine.erase(cmdLine.begin() + 1);
    }
    ScanCalibration scan_cal;
    scan_cal.load(cmdLine[0], options.Y);
}

void get_scatter_stats(string filename) //  file is tif scatter chart
{
    std::cout << "\nAnalyzing Reflection Chart RGB Variations\n";
    PatchCharts patches;
    validate(file_is_tif(filename), "Scanned target image must be \".tif\" file");
    patches.init(filename,false);
    std::cout << "File:" << filename << " ,Rows:" << patches.charts[0].rows
        << ", Cols:" << patches.charts[0].cols << "\n";

    array<array<array<Statistics,3>, 6>, 2> stats;      // R G B C Y M and w g9 g8 g7 g6 g5 rgb stats
    Array2D<V3> rgb(35,29,patches.rgb255.data(), false);
    for (int i = 0; i < 2; i++)                     // colors v neutrals
        for (int ii = 0; ii < 6; ii++)              // color/neutral sequence
            for (int iii = 0; iii < 3; iii++)       // backgrounds: white, gray, black
                for (int iiii=0; iiii < 3; iiii++)  // rgb
                {
                    auto x = rgb(iii*11+4 + i*4, ii*4+4)[iiii];
                    stats[i][ii][iiii].clk(x);
                }
    auto pr3 = [&stats](int row, int col, int color) {
            printf("%5.1f", stats[row][col][color].ave());
            printf("%5.1f   ", stats[row][col][color].max()-stats[row][col][color].min());
    };
    printf("\nNeutrals\n");
    printf("   White  d    Gray 9  d    Gray 8  d    Gray 7  d    Gray 6  d    Gray 5  d\n");
    for (int color = 0; color < 3; color++) {
        printf("%c  ", "RGB"[color]);
        for (int i = 0; i < 6; i++)
            pr3(1, i, color);
        printf("\n");
    }
    printf("\nColors\n");
    printf("     Red  d     Green  d      Blue  d      Cyan  d    Yellow  d   Magenta  d\n");
    for (int color = 0; color < 3; color++) {
        printf("%c  ", "RGB"[color]);
        for (int i = 0; i < 6; i++)
            pr3(0, i, color);
        printf("\n");
    }
    Statistics std_cum;
    for (int i = 0; i < 2; i++)                     // colors v neutrals
        for (int ii = 0; ii < 6; ii++)              // color/neutral sequence
            for (int iii = 0; iii < 3; iii++)       // backgrounds: white, gray, black
                std_cum.clk(stats[i][ii][iii].std());
    printf("Average of all channel stds: %5.2f\n", std_cum.ave());
}