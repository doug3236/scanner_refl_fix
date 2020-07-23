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
#include <cstdio>
#include "calibration.h"
#include "statistics.h"
#include "ScannerReflFix.h"
#include "validation.h"

extern Options options;

// Read in special scan of calibration image containing white squares of various
// sizes to calculate a re-reflection matrix and gray squares to calibrate gamma
bool ScanCalibration::load(std::string cal_tif_file, const vector<float>& neutrals_for_gamma)
{
	raw_image_in = TiffRead(cal_tif_file.c_str(), 1);		// Read in tiff file but don't adjust for gamma
	if (raw_image_in.dpi !=200)
		raw_image_in = arrayRGBChangeDPI(raw_image_in, 200);
	DPI=static_cast<float>(raw_image_in.dpi);
	image = Array2D<float>(raw_image_in.nr, raw_image_in.nc);
	//image.print("fileII.txt");
	for (int i = 0; i < image.nr; ++i)
		for (int ii = 0; ii < image.nc; ++ii)
			image[i][ii] = raw_image_in(i, ii, 1);	// use only green pixels
	Extants corners = get_global_extants(image);
	image=image.clip(corners);						// trim to cal reference boundaries
	bool result=update_square_data(true, neutrals_for_gamma);			// initial pass to get accurate gamma values
	printf("Gammas: %4.2f %4.2f %4.2f %4.2f\n", gamma[0], gamma[1], gamma[2], gamma[3]);
	if (*std::min_element(gamma.begin(), gamma.end()) < .50f || *std::max_element(gamma.begin(), gamma.end()) > 3.0f ||
		*std::max_element(gamma.begin(), gamma.end()) - *std::min_element(gamma.begin(), gamma.end()) > .75f)
		throw "gammas too far out of range";
	image.pow(gamma[1]);

	// now with gamma estimate get linear, downsampled square info at 50dpi
	for (int i = 0; i < 14; i++)
	{
		Array2D<float> sq200 = image.extract(extants[i].top - 1, extants[i].bottom - extants[i].top + 2,
			extants[i].left - 1, extants[i].right - extants[i].left + 2);
		Array2D<float> sq100 = downsample(sq200, 2);
		squares_50dpi[i] = downsample(sq100, 2);
		locate_boundaries(i);
	}
	update_square_data(false, neutrals_for_gamma);			// second pass to generate reflection matrix

	FILE* fp = fopen("scanner_cal.txt", "wt");
	fprintf(fp, "scanner \"%s\"  \"%s %s\"\n", cal_tif_file.c_str(), __DATE__, __TIME__);
	fprintf(fp, "gamma %5.3f\n", gamma[1]);
	fprintf(fp, "grid_size 19\n");
	fprintf(fp, "grid_dpi 10\n");
	for (int i = 0; i < reflection10dpi.nr; i++)
	{
		for (int ii = 0; ii < reflection10dpi.nc; ii++)
			fprintf(fp, "%8.5f ", base_gain*reflection10dpi(i, ii));
		fprintf(fp, "\n");
	}
	fclose(fp);
	return true;
}

// Image encoded in RGB gamma=2.2 using Rel Col.
// extract info from squares in black/white scanner calibration target
// There are two passes over this. The first one calculates gamma from
// the series of small, stepped gray, patches.
float ScanCalibration::update_square_data(bool update_gamma, const vector<float>& neutrals_for_gamma) {
	// get dark areas
	Array2D<float> da=image.extract(10, 20, 10, 20);
	dark = std::accumulate(da.v.begin(), da.v.end(),0.0f)/400.0f;

	auto iround =[](float f){return static_cast<int>(f+.5f);};
	for (size_t n = 0; n < locs.size(); n++)
	{
		auto loc=locs[n];
		int locV=inch_2_pix(loc[0]);
		int locH=inch_2_pix(loc[1]);
		int offset1=inch_2_pix(.4f*loc[2]);
		int offset2=inch_2_pix(.6f*loc[2]);
		int tiny=inch_2_pix(.125f);
		int len=2*tiny+inch_2_pix(loc[2]);
		vector<float> strip; strip.reserve(len);
		for (int i = locH - tiny; i < locH - tiny + len; i++)
		{
			float accum=0;
			for (int ii = locV + offset1; ii < locV + offset2; ii++)
				accum+=image(ii,i)/(offset2-offset1);
			strip.push_back(accum);
		}
		// locate the square horizontal boundaries within the search area
		auto thresh=(*min_element(strip.begin(),strip.end())+*max_element(strip.begin(),strip.end()))/2.0f;
		auto d=find_if(strip.begin(),strip.end(),[thresh](float x){return x > thresh;})-strip.begin();
		auto dd=find_if(strip.begin()+d,strip.end(),[thresh](float x){return x <= thresh;})-strip.begin();
		extants[n].left=locH-tiny+static_cast<int>(d);
		extants[n].right=locH-tiny+static_cast<int>(dd);

		strip.clear();
		for (int i = locV - tiny; i < locV - tiny + len; i++)
		{
			float accum=0;
			for (int ii = locH + offset1; ii < locH + offset2; ii++)
				accum+=image(i,ii)/(offset2-offset1);
			strip.push_back(accum);
		}
		// locate the square vertical boundaries within the search area
		thresh=(*min_element(strip.begin(),strip.end())+*max_element(strip.begin(),strip.end()))/2.0f;
		d=find_if(strip.begin(),strip.end(),[thresh](float x){return x > thresh;})-strip.begin();
		dd=find_if(strip.begin()+d,strip.end(),[thresh](float x){return x <= thresh;})-strip.begin();
		extants[n].top=locV-tiny+static_cast<int>(d);
		extants[n].bottom=locV-tiny+static_cast<int>(dd);
		
		// trim 10 pixels around edges to minimize refraction losses
		Extants sqr={extants[n].top+10, extants[n].bottom-10,extants[n].left+10,extants[n].right-10};
		Array2D<float> g = image.clip(sqr);
		refl[n].ave = g.ave();
		int offset_limit = std::min(g.nr, g.nc)/2-2;
		refl[n].slice.resize(offset_limit);
		refl[n].cnt.resize(offset_limit);
		for (int offset = 0; offset < offset_limit; offset++)
		{
			for (int i = offset + 1; i < g.nr - 1 - offset; i++)
			{
				refl[n].slice[offset] += g(i, offset) + g(i, g.nc - offset - 1);
				refl[n].cnt[offset]+=2;
			}
			for (int i = offset; i < g.nc - offset;  i++)
			{
				refl[n].slice[offset] += g(offset, i) + g(g.nr - offset - 1, i);
				refl[n].cnt[offset]+=2;
			}
			refl[n].slice[offset] /= (g.nr-offset*2-2)*2+(g.nc-offset*2-2)*2+4;
		}
	}

	if (update_gamma)
	{
		// This code is a bit convoluted, options are to provide no arguments in which case Y is derived from Rel Col
		// or for 1 argument use that gamma and for 2 or more arguments use Y measurements for gamma.
		// End fill with duplicates of last gamma
		for (int i = 1; i < 5; i++)
		{
			float rgbx = static_cast<float>((255 - 40 * i)) / 255;
			if (neutrals_for_gamma.size()==0)				// calculate gamma based on Rel. Col. Y
				gamma[i-1] = log(pow(rgbx, 2.2f)) / log(refl[i].ave/refl[0].ave);
			else if (neutrals_for_gamma.size()==1)			// use specified gamma
				gamma[i-1] = neutrals_for_gamma[0];
			else if (int(neutrals_for_gamma.size()) > i)
				gamma[i-1] = log(neutrals_for_gamma[i]/neutrals_for_gamma[0]) / log(refl[i].ave / refl[0].ave);
			else
				gamma[i-1] = gamma[i-2];

		}
		return 0;
	}
	else
	{
		// now that we have a gamma estimate and data is put in linear space
		// generate and optimize reflection matrix
		return calculate_reflection_matrix();
	}
}

// find approx DC gain 
float ScanCalibration::optimize_base_gain()
{	vector<Patch2> samples = {
		{0, Patch2::Region::All, 1},		// .15" white patch
		{5, Patch2::Region::All, 1},		// .25" white patch
	};
	// get err associated with 3 white patches for a given DC gain
	auto err = [this, samples](float gain) {
		auto x2 = make_refl_array(gain);
		auto f0 = get_patch_area(samples[0], x2);
		auto f1 = get_patch_area(samples[1], x2);
		auto f2 = get_patch13(gain);		// 3" white patch
		Statistics patches;
		patches.clk(std::get<0>(f0)); patches.clk(std::get<0>(f1)); patches.clk(f2);
		return patches.std();
	};
	float fmin = 0, err_min = err(fmin);
	float fmax = .5, err_max = err(fmax);
	for (int i = 0; i < 15; i++)			// successive approximation for min error 
	{
		if (err_min < err_max)
		{
			fmax = (fmin + fmax) / 2;
			err_max = err(fmax);
		}
		else
		{
			fmin = (fmin + fmax) / 2;
			err_min = err(fmin);
		}
	}
	return (fmin + fmax) / 2;
}

// find exact offsets to locate white patch boundaries
void ScanCalibration::locate_boundaries(int i_patch)
{
	auto& sq = squares_50dpi[i_patch];
	auto middle = sq.nr/2;
	PatchBounds& pb = patch_bounds[i_patch];
	for (int i = 0; i < 5; i++)
		if (sq(i, middle) > .45f)
		{
			pb.left = i+1;
			break;
		}
	for (int i = 0; i < 5; i++)
		if (sq(sq.nr-1-i, middle) > .45f)
		{
			pb.right = sq.nr-2-i;
			break;
		}
	for (int i = 0; i < 5; i++)
		if (sq(middle, i) > .45f)
		{
			pb.top = i+1;
			break;
		}
	for (int i = 0; i < 5; i++)
		if (sq(middle, sq.nc-1-i) > .45f)
		{
			pb.bottom = sq.nc-2-i;
			break;
		}
}

// get mean white patch values in selected areas with pointer to the patch's Array2D
tuple<float, Array2D<float>*>  ScanCalibration::get_patch_area(Patch2 patch,  Array50dpi& reflx)
{
	static int current_square=-1;
	static Array2D<float> ret;
	Statistics patch_region;
	PatchBounds pb=patch_bounds[patch.square];
	if (patch.square != current_square) {
		ret = generate_reflected_light_estimate(squares_50dpi[patch.square], reflx, dark);
		current_square = patch.square;
	}
	if (patch.region == Patch2::Region::All) {
		for (int i = pb.top; i <= pb.bottom; i++)
			for (int ii = pb.left; ii <= pb.right; ii++)
				patch_region.clk(ret(i, ii));
	}
	// This is only useful for larger patch sizes, 10,11,12,13
	else if (patch.region == Patch2::Region::Center) {
		int u = (pb.bottom-pb.top)/4+pb.top;
		int l = 3*(pb.bottom-pb.top)/4+pb.top;
		for (int i = u; i <= l; i++)
			for (int ii = u; ii <= l; ii++)
				patch_region.clk(ret(i, ii));
		}
	else if (patch.region == Patch2::Region::Edges1) {
		for (int i = pb.top; i <= pb.bottom; i++)
			for (int ii = pb.left; ii <= pb.right; ii++)
			{
				if (i == pb.top || i == pb.bottom || ii == pb.left || ii == pb.right)
					patch_region.clk(ret(i, ii));
			}
		}
	else if (patch.region == Patch2::Region::Edges2) {
		for (int i = pb.top; i <= pb.bottom; i++)
			for (int ii = pb.left; ii <= pb.right; ii++)
			{
				if (i <= pb.top+1 || i >= pb.bottom-1 || ii <= pb.left+1 || ii >= pb.right-1)
					patch_region.clk(ret(i, ii));
			}
	}
	return std::make_tuple(patch_region.ave(), &ret);
}

// get average 3.0" white patch center values for maximum re-reflection
float ScanCalibration::get_patch13(float gain)
{
	auto inner_ave = squares_50dpi[13].extract(50, 50, 50, 50).ave();
	return inner_ave - (exp(inner_ave*gain)-1);
}

// returns std (rms of error from patch regions
float ScanCalibration::calibration_quality(bool zero_gain) {
	base_gain = zero_gain ? 0 : optimize_base_gain();
	vector<Patch2> samples = {
		{0, Patch2::Region::All, 1},
		{5, Patch2::Region::All, 1},
		{10, Patch2::Region::All, 1},
		{10, Patch2::Region::Edges2, 1},
		{10, Patch2::Region::Center, 1},
		{11, Patch2::Region::All, 1},
		{11, Patch2::Region::Edges2, 1},
		{11, Patch2::Region::Center, 1},
		{12, Patch2::Region::All, 1},
		{12, Patch2::Region::Edges2, 1},
		{12, Patch2::Region::Center, 1},
		{13, Patch2::Region::All, 1},
		{13, Patch2::Region::Edges2, 1},
		{13, Patch2::Region::Center, 1},
	};
	Statistics patches;
	Array50dpi refl_50 = make_refl_array(base_gain);
	Array2D<float> ret;
	for (auto square : samples)
	{
		//squares_50dpi[square.square]
		auto xxx = get_patch_area(square,refl_50);
		if (options.save_intermediate_files) printf("%6.4f\n", std::get<0>(xxx));
		for (int i = 0; i < square.weight && square.square!=0; i++)
			patches.clk(std::get<0>(xxx));
		string s="sq_"; s += std::to_string(square.square)+".txt";
		if (options.save_intermediate_files) std::get<1>(xxx)->print(s, false);
	}
	float std = patches.std();
	printf("%s %7.5f\n", zero_gain? "Uncorrected reflection rms err" : "Corrected reflection rms err", 255*std);
	return std;
}

// get horizontal and vertical reflectance vectors averaged over .6" centered stripes
void ScanCalibration::calculate_reflection_strips()
{
	auto [top,bottom,left,right] = extants[13];
	int startV = (top+bottom)/2-inch_2_pix(.3f);
	int endV = (top+bottom)/2+inch_2_pix(.3f);
	horiz.clear();
	for (int i = left; i < right; i++)
	{
		float s = 0;
		for (int ii = startV; ii < endV; ii++)
			s += image(ii, i)/(endV-startV);
		horiz.push_back(s);
	}

	vert.clear();
	int startH = (left+right)/2-inch_2_pix(.3f);
	int endH = (left+right)/2+inch_2_pix(.3f);
	vert.clear();
	for (int i = top; i < bottom; i++)
	{
		float s=0;
		for (int ii = startH; ii < endH; ii++)
			s+=image(i,ii)/(endH-startH);
		vert.push_back(s);
	}
}

// get averaged increase values from edges for 10 DPI and base reflectance for 50% re-reflectance
ScanCalibration::Step10dpi ScanCalibration::get_steps_and_align(const vector<float>& horiz, const vector<float>& vert)
{
	auto diff = [](const vector<float>& v) {
		vector<float> ret;
		for (size_t i = 1; i < v.size(); i++)	// average left/right and top/bottom
			ret.push_back(v[i] - v[i - 1]);
		for (size_t i = 0; i < ret.size(); i++)	// force 0 response once <= 0 is detected
			if (ret[i] <= 0)
				for (; i < ret.size(); i++)
					ret[i] = 0;
		return ret;
	};

	vector<float> horizd=diff(horiz);
	vector<float> vertd=diff(vert);

	float horizs=std::accumulate(horizd.begin(), horizd.end(),0.0f);
	float verts=std::accumulate(vertd.begin(), vertd.end(),0.0f);
	float horizadj=verts/((horizs+verts)/2);
	float vertadj=horizs/((horizs+verts)/2);
	std::transform(horizd.begin(),horizd.end(), horizd.begin(), [horizadj](auto v){return horizadj*v;});
	std::transform(vertd.begin(),vertd.end(), vertd.begin(), [vertadj](auto v){return vertadj*v;});
	float base=(horiz[0]+vert[0])/2;
	return Step10dpi{horizd,vertd, base};
}


// using vertical and horizontal step changes generate re-reflection estimation matrix
float ScanCalibration::calculate_reflection_matrix()
{
	float base_refl = refl[0].ave-(.35f*(refl[5].ave-refl[0].ave));	// estimate actual media white with all reflections removed

	// get smooth vector of near edge samples at .1" intervals from 0.0:.1:1.0
	auto smooth = [base_refl](const vector<float>& v) {
		auto v1=v;
		std::reverse(begin(v1), end(v1));
		for (size_t i = 0; i < v.size(); i++)
			v1[i] = (v1[i] + v[i]) / 2;
		v1.resize(v.size()/2);
		vector<float> ret;
		auto delta_low_high = *(v1.end()-1) - base_refl;
		//auto center=(log(1+delta_low_high/base_refl))/2;
		auto center=(log(1+delta_low_high))/2;
		ret.push_back(center+base_refl);
		for (size_t i = 10; i < v1.size() - 20; i += 20) {
			auto tmp = std::accumulate(v1.begin() + i, v1.begin() + 20 + i, 0.0f) / 20.0f;
			ret.push_back(base_refl+log(1+(tmp-base_refl)/base_refl));
		}
		ret.resize(11);		// limit to 1" (at 200 DPI);
		return ret;
	};

	calculate_reflection_strips();
	auto xh= smooth(horiz);
	auto xv= smooth(vert);
	auto&& [horizd, vertd, base]  = get_steps_and_align(xh, xv);
	reflection10dpi=Array2D<float>(19,19);
	for (int i = 0; i < 19; i++)
	{
		int offV=abs(i-9);
		for (int ii = 0; ii < 19; ii++)
		{
			int offH=abs(ii-9);
			reflection10dpi(i, ii) = (vertd[offV]*horizd[offH])/(vertd[0]*horizd[0]);
		}
	}
	// reflection10dpi.print("reflMatrix10dpi.txt", true);

	float gain = 1.0f / (reflection10dpi.ave()*reflection10dpi.v.size());	// Normalize reflection matrix to a gain of 1
	std::transform(reflection10dpi.v.begin(), reflection10dpi.v.end(), reflection10dpi.v.begin(), [gain](auto x){return x*gain;});

	float err{};
	err=calibration_quality(true);		// rms error with no reflection correction
	err = calibration_quality();		// rms error after reflection correction
	return err;
}

// expand 10 DPI reflection matrix to 50 DPI
Array50dpi ScanCalibration::make_refl_array(float gain)
{
	Array50dpi ret;
	Array2D<float> tmp = expand(reflection10dpi, 10, 50);
	for (int i = 0; i < 93; i++)
		for (int ii = 0; ii < 93; ii++)
			ret[i][ii]=gain*tmp(i,ii)/25.0f;
	return ret;
}


