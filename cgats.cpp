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
// Disable errors for fread and fwrite posix functions
#define _CRT_SECURE_NO_WARNINGS

#include <fstream>
#include <utility>
#include <sstream>
#include <iomanip>

#include "cgats.h"
#include "validation.h"

#pragma warning(disable : 4996)

namespace cgats_utilities {

    /// <summary>
    /// tokenize a line and return vector of tokens, token may be quoted
    /// </summary>
    /// <param name="s"> string to tokenize</param>
    /// <returns>vector of tokens</returns>
    vector<string> parse(const string& s) {
        std::stringstream ss(s);
        vector<string> ret;
        for (;;) {
            string tmp;
            ss >> std::quoted(tmp);
            if (tmp.size() == 0)
                break;
            ret.push_back(tmp);
        }
        return ret;
    };

    // Parse file returning a vector<vector<string>> tokens. vector<string> tokenizes one line

    /// <summary>
    /// Parse file returning a vector vector of string tokens.
    /// </summary>
    /// <param name="filename">Ascii file with tokens, may have "..." for white space</param>
    /// <returns>vector of vector of tokens</returns>
    vector<vector<string>> tokenize_file(const string& filename)
    {
        std::ifstream infile(filename);
        if (infile.fail())
            throw std::invalid_argument(string("Read Open failed, File: ") + filename);
        string s;
        vector<vector<string>> ret;
        while (getline(infile, s))
        {
            vector<string> line_words = parse(s);
            if (line_words.size() != 0)             // skip empty lines
                ret.push_back(line_words);
        }
        return ret;
    }

    /// <summary>
    /// Parse file and get CGATs structure
    /// </summary>
    /// <param name="filename">CGATs file</param>
    /// <returns>Cgats_data struct</returns>
    Cgats_data populate_cgats(const string& filename)
    {
        Cgats_data data;
        data.filename = filename;
        data.lines = tokenize_file(filename);
        data.num_of_fields = stoi(std::find_if(data.lines.begin(), data.lines.end(),
            [](const vector<string>& arg) {return arg[0] == "NUMBER_OF_FIELDS"; })[0][1]);
        data.num_of_sets = stoi(std::find_if(data.lines.begin(), data.lines.end(),
            [](const vector<string>& arg) {return arg[0] == "NUMBER_OF_SETS"; })[0][1]);

        data.fields = std::find_if(data.lines.begin(), data.lines.end(),
            [](const vector<string>& arg) {return arg[0] == "BEGIN_DATA_FORMAT"; })[1];
        validate(data.num_of_fields == data.fields.size(), "Bad CGATS Format, num of fields <> field count");
        data.data_ptr = std::find_if(data.lines.begin(), data.lines.end(),
            [](const vector<string>& arg) {return arg[0] == "BEGIN_DATA"; }) + 1;

        // Note: if RGB_R or LAB_L don't exist returns data.fields.size()
        data.rgb_loc = std::find_if(data.fields.begin(), data.fields.end(),
            [](const string& arg) {return arg == "RGB_R"; }) - data.fields.begin();
        data.lab_loc = std::find_if(data.fields.begin(), data.fields.end(),
            [](const string& arg) {return arg == "LAB_L"; }) - data.fields.begin();
        return data;
    }

    /// <summary>
    /// Read CGATs file RGB values and optional LAB values. Returned values in vector of V6
    /// where V6[0:2]=RGB and V6[3:5]=optional LAB values
    /// note: V6 is array of 6 doubles
    /// </summary>
    /// <param name="filename">CGATs file with RGB_R and optionally LAB_L</param>
    /// <param name="include_lab">true to include LAB values</param>
    /// <returns>vector of V6</returns>
    vector<V6> read_cgats_rgblab(const string& filename, bool include_lab)
    {
        Cgats_data data = populate_cgats(filename);
        vector<V6> rgb_lab(data.num_of_sets);
        for (size_t i = 0; i < rgb_lab.size(); ++i)
        {
            for (size_t ii = 0; ii < 3; ++ii)
            {
                validate(data.data_ptr[i].size() == data.fields.size(), "Bad CGATS Format, num of vals <> field count");
                rgb_lab[i][ii] = stod(data.data_ptr[i][data.rgb_loc + ii]);
                if (include_lab && data.num_of_fields != data.lab_loc)
                    rgb_lab[i][ii+3] = stod(data.data_ptr[i][data.lab_loc + ii]);
            }
        }
        return rgb_lab;
    }

    /// <summary>
    /// This reads only the RGB components of a CGATs file
    /// </summary>
    /// <param name="filename">CGATs file containing RGB values</param>
    /// <returns>vector of V3</returns>
    vector<V3> read_cgats_rgb(const string& filename)
    {
        vector<V6> rgblab = read_cgats_rgblab(filename, false);
        vector<V3> ret;
        for (auto x : rgblab)
            ret.emplace_back(V3{ x[0], x[1], x[2] });
        return ret;
    }

    /// <summary>
    /// Writes RGB CGATs file
    /// </summary>
    /// <param name="rgb">RGB vector</param>
    /// <param name="filename">file to create</param>
    /// <returns>success, throws on error</returns>
    bool write_cgats_rgb(const vector<V3>& rgb, const string& filename)
    {
        FILE* fp = fopen(filename.c_str(), "w");
        if (!fp)
            throw std::invalid_argument("File Open for WRite Failed.");
        fprintf(fp, "NUMBER_OF_FIELDS 4\n"
            "BEGIN_DATA_FORMAT\n"
            "SampleID	RGB_R	RGB_G	RGB_B\n"
            "END_DATA_FORMAT\n\n"
            "NUMBER_OF_SETS	%d\n"
            "BEGIN_DATA\n", static_cast<int>(rgb.size()));
        for (unsigned int i = 0; i < rgb.size(); i++)
            fprintf(fp, "%d\t%6.2f\t%6.2f\t%6.2f\n", i + 1, rgb[i][0], rgb[i][1], rgb[i][2]);
        fprintf(fp, "END_DATA\n");
        fclose(fp);
        return true;
    }

    /// <summary>
    /// Writes minimalist (288 patches) LAB CGATs file in Monaco Reflective scanner format
    /// Specialized function for creating IT8 RGB/LAB reference files
    /// </summary>
    /// <param name="lab"></param>
    /// <param name="filename"></param>
    /// <returns></returns>
    bool write_cgats_lab(const vector<V3>& lab, const string& filename)
    {
        validate(lab.size() == 288, "Lab size must be 288 for Monaco format");
        vector<string> header = {
            "IT8.7/2",
            "DESCRIPTOR           ""IT8.7/2 Reference Data File""",
            "NUMBER_OF_FIELDS 3",
            "BEGIN_DATA_FORMAT",
            "SAMPLE_ID               LAB_L   LAB_A   LAB_B",
            "END_DATA_FORMAT",
            "NUMBER_OF_SETS 288",
            "BEGIN_DATA",
            "#ID       L       A       B"
        };
        vector<string> ids = {
                "A01", "A02", "A03", "A04", "A05", "A06", "A07", "A08", "A09", "A10",
                "A11", "A12", "A13", "A14", "A15", "A16", "A17", "A18", "A19", "A20",
                "A21", "A22", "B01", "B02", "B03", "B04", "B05", "B06", "B07", "B08",
                "B09", "B10", "B11", "B12", "B13", "B14", "B15", "B16", "B17", "B18",
                "B19", "B20", "B21", "B22", "C01", "C02", "C03", "C04", "C05", "C06",
                "C07", "C08", "C09", "C10", "C11", "C12", "C13", "C14", "C15", "C16",
                "C17", "C18", "C19", "C20", "C21", "C22", "D01", "D02", "D03", "D04",
                "D05", "D06", "D07", "D08", "D09", "D10", "D11", "D12", "D13", "D14",
                "D15", "D16", "D17", "D18", "D19", "D20", "D21", "D22", "E01", "E02",
                "E03", "E04", "E05", "E06", "E07", "E08", "E09", "E10", "E11", "E12",
                "E13", "E14", "E15", "E16", "E17", "E18", "E19", "E20", "E21", "E22",
                "F01", "F02", "F03", "F04", "F05", "F06", "F07", "F08", "F09", "F10",
                "F11", "F12", "F13", "F14", "F15", "F16", "F17", "F18", "F19", "F20",
                "F21", "F22", "G01", "G02", "G03", "G04", "G05", "G06", "G07", "G08",
                "G09", "G10", "G11", "G12", "G13", "G14", "G15", "G16", "G17", "G18",
                "G19", "G20", "G21", "G22", "H01", "H02", "H03", "H04", "H05", "H06",
                "H07", "H08", "H09", "H10", "H11", "H12", "H13", "H14", "H15", "H16",
                "H17", "H18", "H19", "H20", "H21", "H22", "I01", "I02", "I03", "I04",
                "I05", "I06", "I07", "I08", "I09", "I10", "I11", "I12", "I13", "I14",
                "I15", "I16", "I17", "I18", "I19", "I20", "I21", "I22", "J01", "J02",
                "J03", "J04", "J05", "J06", "J07", "J08", "J09", "J10", "J11", "J12",
                "J13", "J14", "J15", "J16", "J17", "J18", "J19", "J20", "J21", "J22",
                "K01", "K02", "K03", "K04", "K05", "K06", "K07", "K08", "K09", "K10",
                "K11", "K12", "K13", "K14", "K15", "K16", "K17", "K18", "K19", "K20",
                "K21", "K22", "L01", "L02", "L03", "L04", "L05", "L06", "L07", "L08",
                "L09", "L10", "L11", "L12", "L13", "L14", "L15", "L16", "L17", "L18",
                "L19", "L20", "L21", "L22", "Dmin", "GS01", "GS02", "GS03", "GS04", "GS05",
                "GS06", "GS07", "GS08", "GS09", "GS10", "GS11", "GS12", "GS13", "GS14", "GS15",
                "GS16", "GS17", "GS18", "GS19", "GS20", "GS21", "GS22", "Dmax" };
        FILE* fp = fopen(filename.c_str(), "w");
        if (!fp)
            throw std::invalid_argument("File Open for WRite Failed.");
        for (auto x : header)
            fprintf(fp, "%s\n", x.c_str());
        for (size_t i=0; i < 288; i++)
            fprintf(fp, "%s    %6.2f    %6.2f    %6.2f\n", ids[i].c_str(),
                lab[i][0], lab[i][1], lab[i][2]);
        fprintf(fp, "END_DATA\n");
        fclose(fp);
        return true;
    }

    /// <summary>
    /// Writes CGATs file containing RGB and LAB fields only - No spectral data
    /// </summary>
    /// <param name="rgblab"></param>
    /// <param name="filename"></param>
    /// <param name="descriptor"></param>
    /// <returns>true, throws on error</returns>
    bool write_cgats_rgblab(const vector<V6>& rgblab, const string& filename, string descriptor)
    {
        FILE* fp = fopen(filename.c_str(), "w");
        if (!fp)
            throw std::invalid_argument("File Open for WRite Failed.");
        fprintf(fp,
            "CGATS.17\n"
            "NUMBER_OF_FIELDS 7\n"
            "BEGIN_DATA_FORMAT\n"
            "SAMPLE_ID RGB_R RGB_G RGB_B LAB_L LAB_A LAB_B\n"
            "END_DATA_FORMAT\n"
            "NUMBER_OF_SETS %d\n"
            "BEGIN_DATA\n", static_cast<int>(rgblab.size()));

        for (unsigned int i = 0; i < rgblab.size(); i++)
            fprintf(fp, "%d\t%6.2f\t%6.2f\t%6.2f\t%6.2f\t%6.2f\t%6.2f\n", i + 1,
                rgblab[i][0], rgblab[i][1], rgblab[i][2],
                rgblab[i][3], rgblab[i][4], rgblab[i][5]);
        fprintf(fp, "END_DATA\n");
        fclose(fp);
        return true;
    }

    /// <summary>
    /// Separate out RGB and LAB vectors, use auto [rgb, lab] = separate_rgb_lab(rgblab)
    /// for convenient structured binding of RGB and LAB vectors
    /// </summary>
    /// <param name="rgblab"></param>
    /// <returns>RGB and LAB vectors</returns>
    pair<vector<V3>, vector<V3>> separate_rgb_lab(const vector<V6>& rgblab)
    {
        pair<vector<V3>, vector<V3>> ret;
        ret.first.reserve(rgblab.size()); ret.second.reserve(rgblab.size());
        for (auto x : rgblab)
        {
            ret.first.emplace_back(V3{ x[0], x[1], x[2] });
            ret.second.emplace_back(V3{ x[3], x[4], x[5] });
        }
        return ret;
    }

    /// <summary>
    /// Combine rgb and lab vectors
    /// </summary>
    /// <param name="rgb"></param>
    /// <param name="lab"></param>
    /// <returns>vector of V6, (RGBLAB)</returns>
    vector<V6> combine_rgb_lab(const vector<V3>& rgb, const vector<V3>& lab)
    {
        if (rgb.size() != lab.size())
            throw std::runtime_error("rgb and lab vectors must be same size");
        vector<V6> ret;
        ret.reserve(rgb.size());
        for (unsigned int i = 0; i < rgb.size(); i++)
            ret.emplace_back(V6{ rgb[i][0], rgb[i][1], rgb[i][2], lab[i][0], lab[i][1] , lab[i][2] });
        return ret;
    }

    // predicate for sort by RGB values
    bool less_than(const V6& arg1, const V6& arg2) {
        for (int i = 0; i < 3; i++)
        {
            if (arg1[i] < arg2[i])
                return true;
            else if (arg1[i] > arg2[i])
                return false;
        }
        return false;
    }

    // Overrides operator== for array<> to look at only the first 3 values
    bool operator==(const V6& arg1, const V6& arg2) {
        for (int i = 0; i < 3; i++)
            if (arg1[i] != arg2[i])
                return false;
        return true;
    }

    // Sorts b&W patches, averages Labs, and removes duplicates
    vector<DuplicateStats> remove_duplicates(vector<V6> vals)
    {
        vector<DuplicateStats> ret;
        sort(vals.begin(), vals.end(), less_than);
        for (size_t i = 0; i < vals.size(); i++)
        {
            DuplicateStats same;
            same.rgb_lab = vals[i];
            same.lab[0].clk(vals[i][3]);
            same.lab[1].clk(vals[i][4]);
            same.lab[2].clk(vals[i][5]);
            for (; i < vals.size() - 1 && vals[i][0] == vals[i + 1][0]; i++)
            {
                same.lab[0].clk(vals[i + 1][3]);
                same.lab[1].clk(vals[i + 1][4]);
                same.lab[2].clk(vals[i + 1][5]);
            }
            same.rgb_lab[3] = same.lab[0].ave();
            same.rgb_lab[4] = same.lab[1].ave();
            same.rgb_lab[5] = same.lab[2].ave();
            ret.push_back(same);
        }
        return ret;
    }
}
