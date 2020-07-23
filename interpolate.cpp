// interpolate.cpp : This file contains the 'main' function. Program execution begins and ends there.
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

#define _CRT_SECURE_NO_WARNINGS
#include <filesystem>
#include <algorithm>

#include "interpolate.h"
#include "tiffresults.h"
#include "ScannerReflFix.h"

extern Options options;

// for debugging and printing matrixes for checking with Matlab code
void print(Array2D<double> & v)
{
	for (int i = 0; i < v.nc; i++)
	{
		for (int ii = 0; ii < v.nc; ii++) {
			auto cc = v[i][ii];
			printf("%6.4f ", 100*v[i][ii]);
		}
		printf("\n");
	}
}


// tokenize a line and return vector of tokens, token may be quoted
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


// create interpolation matrix with dpi_out resolution
Array2D<float> InterpolateRefl::get_interpolation_array(int dpi_out) {

	// expand to requested out_dpi and adjust for overall reflectance
	Array2D<float> ex = expand(adj, dpi_in, static_cast<float>(dpi_out));

	// move into center of 1" matrix for compatibility with ScannerReflFix
	Array2D<float> out(dpi_out*2+1,dpi_out*2+1);
	int d=(out.nc-ex.nc)/2;		// assumes nc==nr
	if (d <=0)
		throw "Reflectance matrix is not inside 1 in boundary";
	for (int i=0; i < ex.nr; i++)
		for (int ii = 0; ii < ex.nc; ii++)
		{
			out[i+d][ii+d] = ex[i][ii];
		}
	out.scale((adj.ave()*adj.v.size())/(out.ave()*out.v.size()));
	auto xx1=adj.ave();
	auto xx2=ex.ave();
	return out;
}


// Initialize reflection interpolation quadrant from external file or default
bool InterpolateRefl::read_init_file(string filename, bool print) {
	if (!std::filesystem::exists(filename) && std::filesystem::exists(options.executable_directory+filename))
		filename=options.executable_directory+filename;
	if (std::filesystem::exists(filename)){
		if (print)
			std::cout << "Using Adjustment file: " << filename << "\n";
		vector<vector<string>> file_data = tokenize_file(filename);
		if (file_data[0][0]!= "scanner")
			throw "Scanner adj. file not recognized";
		adj_file=file_data[0][1];
		if (file_data[0].size() == 3)
			printf("Calibration file: %s\n", file_data[0][2].c_str());
		file_data.erase(file_data.begin());
		if (file_data[0][0]!= "gamma" || file_data[1][0]!= "grid_size" || file_data[2][0]!= "grid_dpi")
			throw ("Initialization file format error");
		gamma = static_cast<float>(stod(file_data[0][1]));
		grid_size = stoi(file_data[1][1]);
		dpi_in = stof(file_data[2][1]);
		adj = Array2D<float>(grid_size,grid_size);
		for (int i = 0; i < grid_size; i++)
			for (int ii = 0; ii < grid_size; ii++)
				adj[i][ii]=stof(file_data[3+i][ii]);
		gain_adj=adj.ave()*adj.nr*adj.nc;
		if (print)
			printf("Reflected light gain: %4.1f%%,  Gamma=%4.2f\n", 100.0f*gain_adj, gamma);
	}
	else
		throw "scanner_cal.txt not detected";
	return true;
}


// https://en.wikipedia.org/wiki/Bilinear_interpolation
Array2D<float> expand(Array2D<float>& adj, float dpi_in, float dpi_out)
{
	int new_dist = 1+2*int((dpi_out/dpi_in)*(adj.nc-1)/2+1);		// adj is odd sized square matrix where center is nominally 0,0
	Array2D<float> ret(new_dist,new_dist);
	int center_dest=new_dist/2;
	int center_from=adj.nc/2;
	for (int i = 1; i < new_dist-1; i++)
		for (int ii = 1; ii < new_dist-1; ii++)
		{
			float offv = .9999f*static_cast<float>(i-center_dest)*dpi_in/dpi_out;		// .9999 prevents slight fract over cell boundary
			float offh = .9999f*static_cast<float>(ii-center_dest)*dpi_in/dpi_out;
			int offv0 = (int)floor(offv);
			float dr=offv-offv0;
			int offh0 = (int)floor(offh);
			float dc = offh-offh0;
			float q00 = adj[offv0+center_from][offh0+center_from];
			float q01 = adj[offv0+center_from][offh0+center_from+1];
			float q10 = adj[offv0+center_from+1][offh0+center_from];
			float q11 = adj[offv0+center_from+1][offh0+center_from+1];
			auto v = q00*(1-dr)*(1-dc)+q10*dr*(1-dc)+q01*(1-dr)*dc+q11*dr*dc;
			ret[i][ii]= v;
		}
	return ret;
}

Extants get_global_extants(const Array2D<float> &image)
{
	Extants corners;
	int b = image.nr;
	int r = image.nc;
	for (int i = 0; i < b/2; i++)
		if (image[i][r/2] < .5f) {
			corners.top = i;
			break;
		}
	for (int i = b-1; i > b/2; i--)
		if (image[i][r/2] < .5f) {
			corners.bottom = i;
			break;
		}
	for (int i = 0; i < r/2; i++)
		if (image[b/2][i] < .5f) {
			corners.left = i;
			break;
		}
	for (int i = r-1; i > r/2; i--)
		if (image[b/2][i] < .5f) {
			corners.right = i;
			break;
		}
	return corners;
}

