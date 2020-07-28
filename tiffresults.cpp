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

#include "tiffresults.h"
#include <memory>
#include <array>
#include <string>
#include <fstream>
#include <iostream>
#include <algorithm>
#include "interpolate.h"


ArrayRGB arrayRGBChangeDPI(const ArrayRGB& imag_in, int new_dpi)
{
    auto maxlen = [](float step, int rc_len) {      // crude but always correct
        for (int i = 0;; i++)
            if (static_cast<int>(step * i) >= rc_len)
                return i - 1;
    };
    float step =  imag_in.dpi / static_cast<float>(new_dpi);
    ArrayRGB imag_out(maxlen(step, imag_in.nr), maxlen(step, imag_in.nc), new_dpi, true, imag_in.gamma);
    for (int color = 0; color < 3; color++)
            for (int row = 0; row < imag_out.nr; row++)
                for (int col = 0; col < imag_out.nc; col++)
                    imag_out(row, col, color) = imag_in(static_cast<int>(step * row), static_cast<int>(step * col), color);
    return imag_out;
}



// Reads a tiff file and returns image in linear space (gamma=1) scaled 0-1
ArrayRGB TiffRead(const char *filename, float gamma)
{
    ArrayRGB rgb;               // ArrayRGB to be returned
    uint32 prof_size = 0;       // size of byte arrray for storing profile if present
    uint8 *prof_data = nullptr; // ptr to byte array
    uint16 bits;                // image was from 8 or 16 bit tiff
    uint32 height;              // image pixe sizes
    uint32 width;
    uint32 size;                // width*height
    uint16 planarconfig;        // pixel tiff storage orientation
    float local_dpi;

    vector<uint32> image;
    TIFF *tif;

    tif = TIFFOpen(filename, "r");
    if (tif == 0)
    {
        return rgb;
    }
    TIFFGetField(tif, TIFFTAG_BITSPERSAMPLE, &bits);
    TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &width);
    TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &height);
    TIFFGetField(tif, TIFFTAG_XRESOLUTION, &local_dpi);       // assume Xand Y the same
    TIFFGetField(tif, TIFFTAG_ICCPROFILE, &prof_size, &prof_data);
    TIFFGetField(tif, TIFFTAG_PLANARCONFIG, &planarconfig);
    if (prof_size!=0)
    {
        rgb.profile.resize(prof_size);
        memcpy(rgb.profile.data(), prof_data, prof_size);
    }

    size = width*height;
    rgb.resize(height, width);
    rgb.nc = width;
    rgb.nr = height;
    rgb.dpi = (int)local_dpi;
    rgb.gamma = gamma;
    if (planarconfig != PLANARCONFIG_CONTIG || bits == 8) {
        if (bits == 16)
            std::cout << "16 bit tif file not recognized, reverting to 8 bit read.\n";
        rgb.from_16bits = false;
        image.resize(height*width);
        int istatus = TIFFReadRGBAImage(tif, width, height, image.data());
        if (istatus==1) {
            for (uint32 c = 0; c < width; c++)
            {
                for (uint32 r = 0; r < height; r++)
                {
                    int yt = height-r-1;
                    //auto z0 = this->operator[](yt)[c];
                    uint32 z0 = image[yt*width+c];
                    rgb(r, c, 0) = pow(static_cast<float>(z0 & 0xff)/255, gamma);
                    rgb(r, c, 1) = pow(static_cast<float>((z0>>8) & 0xff)/255, gamma);
                    rgb(r, c, 2) = pow(static_cast<float>((z0>>16) & 0xff)/255, gamma);
                }
            }
        }
        else
            throw "Bad TIFFReadRGBAImage";
    }
    else
    {
        rgb.from_16bits = true;
        uint32 row, col;
        uint16 config, nsamples;

        TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &height);
        TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &width);
        TIFFGetField(tif, TIFFTAG_PLANARCONFIG, &config);
        TIFFGetField(tif, TIFFTAG_SAMPLESPERPIXEL, &nsamples);
        vector<uint16_t> bufs(TIFFScanlineSize(tif));
        if (config == PLANARCONFIG_CONTIG) {
            for (row = 0; row < height; row++)
            {
                tmsize_t count = TIFFReadScanline(tif, bufs.data(), row);
                for (col = 0; col < width; col++)
                {
                    rgb(row, col, 0) = pow(1.0f*bufs[col*nsamples + 0]/65535, gamma);
                    rgb(row, col, 1) = pow(1.0f*bufs[col*nsamples + 1] / 65535, gamma);
                    rgb(row, col, 2) = pow(1.0f*bufs[col*nsamples + 2] / 65535, gamma);
                }
            }
        }
        else
            throw "16 bit file type not supported";
    }
    return rgb;
}

void attach_profile(const std::string & profile, TIFF * out, const ArrayRGB & rgb)
{
    // If profile is requested, read the profile file and store it in tiff image.
    if (profile != "")
    {
        ifstream profilestream(profile, ios::binary);
        if (profilestream.fail())
            throw "File could not be opened";
        vector<char>  profileimage;
        profilestream.seekg(0, ios_base::end);
        size_t size = (size_t)profilestream.tellg();
        profilestream.seekg(0, ios_base::beg);
        profileimage.resize(size);
        profilestream.read(profileimage.data(), size);
        TIFFSetField(out, TIFFTAG_ICCPROFILE, (uint32)size, profileimage.data());
    }
    // rgb image already has a profile save it to new tiff
    else if (rgb.profile.size() != 0)
    {
        TIFFSetField(out, TIFFTAG_ICCPROFILE, (uint32)(rgb.profile.size()), rgb.profile.data());
    }
}

void TiffWrite(const char* file, const Array2D<float> rgb)
{
    ArrayRGB rgb3(rgb.nr, rgb.nc, 200, false, 1.0);
    for (int i = 0; i < rgb.nr; i++)
        for (int ii = 0; ii < rgb.nc; ii++)
            rgb3(i,ii,0)=rgb3(i,ii,1)=rgb3(i,ii,2)=rgb(i,ii);
    TiffWrite(file, rgb3,"");
}

void TiffWrite(const char *file, const ArrayRGB &rgb, const string &profile)
{
    float gamma = rgb.gamma;
    int sampleperpixel=3;
    TIFF *out = TIFFOpen(file, "w");
    TIFFSetField(out, TIFFTAG_IMAGEWIDTH, rgb.nc);  // set the width of the image
    TIFFSetField(out, TIFFTAG_IMAGELENGTH, rgb.nr);    // set the height of the image
    TIFFSetField(out, TIFFTAG_SAMPLESPERPIXEL, sampleperpixel);   // set number of channels per pixel
    TIFFSetField(out, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);    // set the origin of the image.
                                                                    //   Some other essential fields to set that you do not have to understand for now.
    TIFFSetField(out, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
    TIFFSetField(out, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
    TIFFSetField(out, TIFFTAG_XRESOLUTION, (float)rgb.dpi);
    TIFFSetField(out, TIFFTAG_YRESOLUTION, (float)rgb.dpi);
    attach_profile(profile, out, rgb);
    if (!rgb.from_16bits)
    {
        TIFFSetField(out, TIFFTAG_BITSPERSAMPLE, 8);    // set the size of the channels
        tsize_t linebytes = sampleperpixel * rgb.nc;
        vector<unsigned char> buf(linebytes);

        // We set the strip size of the file to be size of one row of pixels
        TIFFSetField(out, TIFFTAG_ROWSPERSTRIP, TIFFDefaultStripSize(out, rgb.nc*sampleperpixel));
        vector<array<uint8, 3>> image(rgb.nr*rgb.nc);
        //unique_ptr<uint32[]> image(new uint32[rgb.nr*rgb.nc]);
        auto igamma = 1 / gamma;
        auto one_channel_to_8 = [](const vector<float> &image_ch, int cols, float inv_gamma) {
            float resid = 0;
            vector<uint8> ret(image_ch.size());
            for (size_t i = 0; i < ret.size(); i++)
            {
                if (i % cols == 0)    // No offset at start of each row
                    resid = 0;
                float tmp = 255 * pow(image_ch[i], inv_gamma);
                if (tmp > 255) tmp = 255;
                if (tmp < 0) tmp = 0;
                uint8 tmpr = static_cast<uint8>(tmp + .5);
                resid += tmp - tmpr;
                if (resid > .5 && tmpr < 255)
                {
                    resid -= 1;
                    tmpr++;
                }
                else if (resid < -.5)
                {
                    resid += 1;
                    tmpr--;
                }
                ret[i] = tmpr;
            }
            return ret;
        };
        vector<uint8> red = one_channel_to_8(rgb.v[0], rgb.nc, igamma);
        vector<uint8> green = one_channel_to_8(rgb.v[1], rgb.nc, igamma);
        vector<uint8> blue = one_channel_to_8(rgb.v[2], rgb.nc, igamma);

        for (int r = 0; r < rgb.nr; r++)
            for (int c = 0; c < rgb.nc; c++)
            {
                int offset = r * rgb.nc + c;
                //image[offset] = red[offset] | green[offset] << 8 | blue[offset] << 16 | 0xff000000;
                image[offset][0] = red[offset];
                image[offset][1] = green[offset];
                image[offset][2] = blue[offset];
            }

        //Now writing image to the file one strip at a time
        for (int row = 0; row < rgb.nr; row++)
        {
            //memcpy(buf, &image[row*linebytes/sizeof(uint32)], linebytes);    // check the index here, and figure out why not using h*linebytes
            memcpy(buf.data(), &image[row*rgb.nc], linebytes);    // check the index here, and figure out why not using h*linebytes
            if (TIFFWriteScanline(out, buf.data(), row, 0) < 0)
                break;
        }
        TIFFClose(out);
    }
    else
    {
        TIFFSetField(out, TIFFTAG_BITSPERSAMPLE, 16);    // set the size of the channels
        // 16  bit write
        vector<array<uint16, 3>> buf(rgb.nc);
        // We set the strip size of the file to be size of one row of pixels
        TIFFSetField(out, TIFFTAG_ROWSPERSTRIP, TIFFDefaultStripSize(out, rgb.nc*sampleperpixel));

        auto igamma = 1 / gamma;

        for (int r = 0; r < rgb.nr; r++)
        {
            for (int c = 0; c < rgb.nc; c++)
            {
                for (int color = 0; color < 3; color++)
                {
                    buf[c][color] = static_cast<uint16>(pow(std::clamp(rgb(r, c, color), 0.f, 1.f), igamma) * 65535);
                }
            }
            if (TIFFWriteScanline(out, buf.data(), r, 0) < 0)
                throw "Error writing tif";
        }
        TIFFClose(out);
    }
}



void ArrayRGB::fill(float red, float green, float blue) {
    for (auto& x:v[0]) { x = red; }
    for (auto& x:v[1]) { x = green; }
    for (auto& x:v[2]) { x = blue; }
}

void ArrayRGB::copy(const ArrayRGB &from, int offsetx, int offsety)
{
    if (from.nr + offsetx > nr || from.nc + offsety > nc)
        throw std::invalid_argument("copy with out of range arguments");
    //assert(from.nr + offsetx <= nr);
    //assert(from.nc + offsety <= nc);
    for (int color = 0; color < 3; color++)
        for (int x = 0; x < from.nr; x++)
            for (int y = 0; y < from.nc; y++)
                (*this)(x+offsetx, y+offsety, color) = from(x, y, color);
}

// note: re (row end) and ce (column end) are included, not one past
ArrayRGB ArrayRGB::subArray(int rs, int re, int cs, int ce)
{
    if (!(rs <= re && re < nr) || !(cs <= ce && ce < nc))
        throw std::invalid_argument("copy with out of range arguments");
    //assert(rs <= re && re < nr);
    //assert(cs <= ce && ce < nc);
    ArrayRGB s(re-rs+1, ce-cs+1);
    for (int color = 0; color < 3; color++)
        for (int r = rs; r <= re; r++)
            for (int c = cs; c <= ce; c++)
                s(r-rs, c-cs, color) = (*this)(r, c, color);
    return s;
}

void ArrayRGB::copyColumn(int to, int from)
{
    for (int color = 0; color < 3; color++)
        for (int r = 0; r < nr; r++)
            (*this)(r, to, color) = (*this)(r, from, color);
}

void ArrayRGB::copyRow(int to, int from)
{
    for (int color = 0; color < 3; color++)
        for (int c = 0; c < nc; c++)
            (*this)(to, c, color) = (*this)(from, c, color);
}

void ArrayRGB::scale(float factor)    // scale all array values by factor
{
    for (int i = 0; i < 3; i++)
        for (auto &x:v[i])
            x *= factor;
}

#pragma optimize("t", on)
// return std::make_tuple(ret, x2, x3);
tuple<ArrayRGB, int, int> getReflArea(const int dpi, InterpolateRefl& interpolate, const int use_this_size_if_not_0)
{
    auto actual_dpi = !use_this_size_if_not_0 ? dpi : use_this_size_if_not_0;
    float gain = 1;
    int x2 = 0;
    int x3 = 0;
    if (!use_this_size_if_not_0)		// find smaller size for faster interpolation (normal usage)
    {
        while (actual_dpi >= 90 && actual_dpi % 3 == 0)
        {
            actual_dpi /= 3;
            x3++;
        }
        while (actual_dpi >= 60 && actual_dpi % 2 == 0)
        {
            actual_dpi /= 2;
            x2++;
        }
    }
    Array2D<float> refl = interpolate.get_interpolation_array(actual_dpi);

    //refl.print("refl.txt");
    ArrayRGB ret(2*actual_dpi+1, 2*actual_dpi+1, actual_dpi);
    for (int i = 0; i < ret.nr; i++)
        for (int ii = 0; ii < ret.nc; ii++)
            ret(i,ii,0)=ret(i,ii,1)=ret(i,ii,2)=refl[i][ii];

    return { ret, x2, x3};
}
#pragma optimize("", on)



// Used ONLY for creating reflection array from scanned squares at 200 DPI.
// where created reflection array is 10DPI and 19x19. std::array used for speed
// Create gray scale interpolated re-reflected values from 50 DPI image
// with 1" surround then remove 1" surround
#pragma optimize("t",on)
Array2D<float> generate_reflected_light_estimate(const Array2D<float>& image_reduced, const array<array<float,93>,93>& refl_area, float fill)
{
    //image_reduced.print("red15.txt", false);
    //Array2D<float> image_correction(image_reduced.nr, image_reduced.nc);
    Array2D<float> image_correction = image_reduced;
    Array2D<float> image_reduced_w_margin = Array2D<float>(image_reduced.nr + 92, image_reduced.nc + 92, fill);
    image_reduced_w_margin.insert(image_reduced, 46, 46);
    for (int i = 0; i < image_reduced_w_margin.nr - 93 + 1; i++)
    {
        for (int ii = 0; ii < image_reduced_w_margin.nc - 93 + 1; ii++)
        {
            float sum = 0;
            for (int j = 0; j < 93; j++)
            {
                for (int jj = 0; jj < 93; jj++)
                {
                    sum += image_reduced_w_margin(i + j, ii + jj)*refl_area[j][jj];
                }
            }
            //image_correction(color, ii) = sum;        //  re-re-reflections not included
            image_correction(i, ii) -= exp(sum)-1;   //  re-re-reflections included
        }
    }
    //image_correction = image_correction.extract(46+3, image_reduced.nr-6, 46+3, image_reduced.nc-6);
    return image_correction;
}
#pragma optimize("",on)


// Create interpolated re-reflected values from original
// remove 1" surround and set DPI at reduced resolution
#pragma optimize("t",on)
ArrayRGB generate_reflected_light_estimate(const ArrayRGB& image_reduced, const ArrayRGB& refl_area)
{
	ArrayRGB image_correction = ArrayRGB(image_reduced.nr - 2 * image_reduced.dpi,
		image_reduced.nc - 2 * image_reduced.dpi,
		image_reduced.dpi,
		image_reduced.from_16bits,
		image_reduced.gamma
	);

	auto fix = [&image_reduced, &refl_area, &image_correction](int s_row, int e_row, int color) {
		for (int i = s_row; i < e_row; i++)
		{
			for (int ii = 0; ii < image_reduced.nc - refl_area.nc + 1; ii++)
			{
				float sum = 0;
				for (int j = 0; j < refl_area.nr; j++)
				{
					for (int jj = 0; jj < refl_area.nc; jj++)
					{
						sum += image_reduced(i + j, ii + jj, color)*refl_area(j, jj, color);
					}
				}
                // second order effect (reflections of reflections) included
                image_correction(i, ii, color) = exp(sum)-1;
			}
		}
	};
	auto c0 = async(launchType, fix, 0, image_reduced.nr - refl_area.nr + 1, 0);
	auto c1 = async(launchType, fix, 0, image_reduced.nr - refl_area.nr + 1, 1);
	auto c2 = async(launchType, fix, 0, image_reduced.nr - refl_area.nr + 1, 2);
	c0.get(); c1.get(); c2.get();
	return image_correction;
}
#pragma optimize("",on)
