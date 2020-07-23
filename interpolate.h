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
#include <array>
#include <numeric>
#include "statistics.h"

using std::vector;
using std::string;
using std::array;
using std::tuple;

// used for sub-image boundaries
struct Extants {
	int top;
	int bottom;
	int left;
	int right;
};


// General 2D array suitable for working with single color images
template <class T>
class Array2D {
public:
	vector<T> v;
	int nc;
	int nr;
	Array2D(int NR = 0, int NC = 0) : v(NR* NC), nc(NC), nr(NR) {}
	Array2D(int NR, int NC, T val) : v(NR* NC, val), nc(NC), nr(NR) {}
	Array2D(int NR, int NC, T *val, bool transpose);	// transposed=true to switch from col major to row major
	T* operator[](int r) { return &v[r * nc]; }
	const T* operator[](int r) const { return &v[r * nc]; }
	T& operator()(int i, int j) { return v[i * nc + j]; }
	const T& operator()(int i, int j) const { return v[i * nc + j]; }
	void pow(float power) { std::transform(v.begin(), v.end(), v.begin(), [power](T x) {return std::pow(x, power); }); }
	Array2D<T> clip(Extants bounds);
	T ave() { return std::accumulate(v.begin(), v.end(), T{ 0 }) / v.size(); };
	void print(std::string filename, bool normalize=true) const;
	Array2D<T> extract(int rowstart, int rlen, int colstart, int clen);
	void insert(const Array2D<T>& from, int start_row, int start_col);
	void fill(T val){std::generate(v.begin(), v.end(), [val]() { return val; });}
	array<T,3> flatness(int offset=3, int width=100);
	void scale(T factor) {std::transform(v.begin(), v.end(), v.begin(), [factor](T x) {return x * factor; });}
	template<int N, int M>
	array<array<float, N>, M> copy_to_array();
};

template<class T>
Array2D<T> transpose(const Array2D<T>& x)
{
	Array2D<T> ret(x.nc, x.nr);
	for (int row = 0; row < ret.nr; row++)
		for (int col = 0; col < ret.nc; col++)
			ret[row][col]=x[col][row];
	return ret;
}

// create from ptr to elements, with row/col major selection
template<class T>
Array2D<T>::Array2D(int NR, int NC, T* val, bool transpose):v(NR*NC),nr(NR),nc(NC)
{
	if (transpose)
		for (int i = 0; i < NR; i++)
			for (int ii = 0; ii < NC; ii++)
				operator()(i,ii)=val[i*NC+ii];
	else
		for (int i = 0; i < NC; i++)
			for (int ii = 0; ii < NR; ii++)
				operator()(ii,i)=val[i*NR+ii];
}

// print 2D array for further analysis
template<class T>
void Array2D<T>::print(std::string filename, bool normalize) const {
	FILE* fp = fopen(filename.c_str(), "wt");
	T maxv = normalize ? *std::max_element(v.begin(), v.end()) : 1.0f;
	for (int i = 0; i < nr; i++)
	{
		for (int ii = 0; ii < nc; ii++)
			fprintf(fp, "%8.5f ", this->operator()(i, ii) / maxv);
		fprintf(fp, "\n");
	}
	fclose(fp);
}


// Extract a 2D array subset
template<class T>
Array2D<T> Array2D<T>::extract(int rowstart, int rlen, int colstart, int clen)
{
	Array2D ret(rlen, clen);
	for (int i = 0; i < rlen; i++)
		for (int ii = 0; ii < clen; ii++)
			ret(i, ii) = operator()(rowstart + i, colstart + ii);
	return ret;
}

// Insert a 2D array into another 2D array
template <class T>
void Array2D<T>::insert(const Array2D<T>& from, int start_row, int start_col)
{
	if (nr < from.nr + start_row || nc < from.nc + start_col)
		throw "Array2D.insert, Out of bounds";
	for (int i = 0; i < from.nr; i++)
		for (int ii = 0; ii < from.nc; ii++)
			operator()(i + start_row, ii + start_col) = from(i, ii);
}

// Get statistics on square patch value consistency starting at "offset" from edges to center
template <class T>
array<T,3> Array2D<T>::flatness(int offset, int width)
{
	Statistics res;
	for (int i=offset; i < nr-offset; i++)
		for (int ii=offset; ii < nc-offset; ii++)
			if (i-offset < width || nr-i-offset <= width ||
				ii-offset < width || nc-ii-offset <= width)
			{
				res.clk(operator()(i,ii));
			}
	return array<T, 3>{res.ave(), res.std(), (T)res.n()};
}


// Copy from Array2D on heap to fixed dim array<array<>>, dims must match
template <class T>
template<int N, int M>
array<array<float, N>, M> Array2D<T>::copy_to_array()
{
	if (nr!=N || nc!= M)
		throw "array dimensions must match";
	array<array<float, N>, M> ret;
	for (int i = 0; i < nr; i++)
		for (int ii = 0; ii < nc; ii++)
			ret[i][ii] = operator()(i,ii);
	return ret;
}

// Clip to subset as set in Extants
template <typename T>
Array2D<T> Array2D<T>::clip(Extants bounds) {
	Array2D<T> ret(bounds.bottom - bounds.top + 1, bounds.right - bounds.left + 1);
	for (int i = 0; i < ret.nr; i++)
		for (int ii = 0; ii < ret.nc; ii++)
			ret(i, ii) = this->operator()(i + bounds.top, ii + bounds.left);
	return ret;
}

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
	bool read_init_file(string filename, bool print=false);	// initialize input file;
	Array2D<float> get_interpolation_array(int dpi_out);
};

Array2D<float> expand(Array2D<float>& adj, float dpi_in, float dpi_out);
Extants get_global_extants(const Array2D<float> &image);

// tokenize a line and return vector of tokens, token may be quoted
vector<string> parse(const string& s);

// Parse file returning a vector<vector<string>> tokens. vector<string> tokenizes one line
vector<vector<string>> tokenize_file(const string& filename);

#endif
