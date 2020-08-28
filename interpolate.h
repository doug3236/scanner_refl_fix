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

#ifndef INTERPOLATE_H
#define INTERPOLATE_H

#define _CRT_SECURE_NO_WARNINGS

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <iomanip>
#include <numeric>
#include "statistics.h"
#include "array2d.h"


// offset must be [0.0:1.0), use to re-center resultant interpolated array
// https://en.wikipedia.org/wiki/Bilinear_interpolation
inline float bilinear(Array2D<float> &from, int r, int c, float factor, float offset=0)
{
	float rx = r*factor+offset;
	float cx = c*factor+offset;
	int r0=static_cast<int>(floor(rx));
	float dr = rx-r0;
	int r1 = r0+1;
	int c0=static_cast<int>(floor(cx));
	float dc=cx-c0;
	int c1 = c0+1;
	if (c1 >= from.nc || r1 >= from.nr)
		return 0;

	float q00= from(r0, c0);
	float q01 = from(r0, c1);
	float q10 = from(r1, c0);
	float q11 = from(r1, c1);
	float v = q00*(1-dr)*(1-dc)+q10*dr*(1-dc)+q01*(1-dr)*dc+q11*dr*dc;
	return v;
}


struct InterpolateRefl {
	std::string adj_file;
	Array2D<float> adj;
	float gain_adj;
	float gamma{};
	int grid_size{};
	float dpi_in{};
	float max_dist{1};			// 1" maximum range
	bool read_init_file(std::string filename, bool print=false);	// initialize input file;
	Array2D<float> get_interpolation_array(int dpi_out);
};

Array2D<float> expand(Array2D<float>& adj, float dpi_in, float dpi_out);
Array2D<float>::Extants get_global_extants(const Array2D<float> &image);

// tokenize a line and return vector of tokens, token may be quoted
std::vector<std::string> parse(const std::string& s);

// Parse file returning a vector<vector<string>> tokens. vector<string> tokenizes one line
std::vector<std::vector<std::string>> tokenize_file(const std::string& filename);

#endif
