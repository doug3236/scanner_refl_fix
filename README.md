# scanner_refl_fix
# scanner_refl_fix
## A Program to correct Large Area Spatial Crosstalk and tools to make ICC profiles using Argyll software

*scanner_refl_fix* provides 2 main functionalities. Firstly, it removes light that is reflected
from surrounding scanner structures which can significantly alter image colors. Secondly, it can produce
high quality scanner ICC profiles using arbitrarily large numbers of patches when used in conjunction with
Graeme Gill's free Argyll software and an XRite I1iSis or I1Pro2 spectrophotometer.

### Removing  large area glare from reflection scans

Scanner glare occurs when light illuminating the document being scanned reflects back to the other surfaces
then back again to the document adding to the illumination already there. This changes the total illumination
level and tint which alters colors seen by the scanner's optics.

Aside from metameric failure, scanner glare is the single largest cause of scanner delta E errors and difficulties making good profiles. Worse,
large area glare cannot be corrected by ICC profiles because it causes big variations in the scanned RGB values
before they ever get to a profile. Correcting for this by allows significantly more accurate profiles to be made providing
consistent, colorimetric scans.

Experiments show the worse case is when white regions are surrounded by
large regions of yellow or cyan as shown here:

**Surrounding colors induce error shifting<br> white patches toward surrounding color**<br>![](refs/yellow_cyan.jpg)

These unprinted, "white" patches shown below are 6mm square and show how large this re-reflected light shifts the tints of the white squares.
this was from an Epson V850 scanner. The top two are uncorrected, the bottom two after correction by *scanner_refl_fix* 

**White Patch Reflection Error**<br>![](refs/Reflection_error.jpg)

Removing unwanted reflected light also improves the accuracy of ICC profiles since the RGB patches
in a target patch set are no longer affected by the nearby patch colors. For example, scanning a
randomized set of ColorChecker colors, each duplicated 10 times, improves the ave. delta E 1976
from 1.02 to 0.42 using a 2871 count patch set after the tif images had reflected light removed. Even
profiles made using XRite's IT8 improved a bit from dE 3.92 to 3.47. The three factors that reduced
the IT8 accuracy was:

- Actual errors in measured colors. The IT8 chart was not individually measured and photochemistry is
quite sensitive to small changes when the chart batches are made.
- The small number of patches in an IT8 chart which has just over 200 colors.
- Metameric failure. This is where the scanner's light source and RGB color filters deviate from
the Luther/Ives criteria AND when the spectrum differs between two
supposedly identical colors. For the V850, oranges have quite different spectra between CYMK printed
and photo-chemistry of the IT8 chart.


### This is how it works.

A matrix model of scanner glare (spatial crosstalk) is created and saved as the text file "scanner_cal.txt." This
model characterizes the scanner's reflection glare and is not media sensitive. It only needs to be made once for the
model of scanner. Then, the program can be used remove reflected glare light from scanned tiff files
regardless of the media being scanned. No special tools or other software are needed.

Test charts are provided for measuring the effectiveness of the scanner reflection removal.

Additional options are provided to embed an ICC scanner profile and batch convert a set of tiff files, amongst others.

**Provides tools useful for creating ICC profiles using i1Pro 2 or i1iSis spectrophotometers**

Additionally, printed patch charts can be scanned to make CGATs RGB files. If a CGATs measurement file is available
containing LAB values such as one might get from an XRite i1iSis, or I1 Pro spectrophotometer,
this additional data can be combined to make an CGATs file with scanned RGB and measured LAB values.
This can then be used with open source Argyll tools to create an ICC scanner profile. Charts are provided
with 2871 patches in US Letter size formatted for reading with X-Rite's I1iSiS spectrophotometer.
A smaller patch set is included for use with I1Pro 2


### The Problem in Detail
Reflection desktop scanners have an intrinsic non-linearity from light reflected from nearby image areas.
Lighter portions of a scanned image that are in the immediate surround of the point being scanned
reflect of light in proportion to this region's brightness. Some of this illuminates structures
that are about 10mm from the top and bottom of the slot which is being scanned.
When the image is all or near "white," the light which is reflected from the paper to the frosted surfaces then
back to the paper can be almost 20% of the light originally illuminating the paper. On my Epson V850 scanner the
reflected light from an unprinted sheet of Canson Photographique Matte adds slightly over 20%.

An IEC standard, 61966-8, describes this as "Large Area Spatial Crosstalk."
This causes a scanned image colors to be slightly shifted by the additional reflected light.
Areas surrounded by white will increase in brightness. Areas surrounded by a strong color will
be shifted in the direction of that color.

## Walkthrough of Typical ScannerRefFix uses
### Initially Calibrating the Scanner
Initially one needs to create the scanner reflection model. This is done by printing a
chart, *scan_calibration.tif*, that contains various sized white squares embedded
in black surrounds. From this the light that is reflected from the paper
off the nearby illuminating frosted white components is  modeled as a 2D matrix and stored as the file
"scanner_cal.txt." This file should be printed using Rel. Col. Intent and placed in the same directory
as the executable scanner_refl_fix.exe or in the directory where the program is executed. The paper and printer
used is not important. It is not necessary to use the same paper or even paper type or printer when using
the resulting model. The model is specific only to the scanner.

**Image analyzed to create reflection correction data<br>**
*scan_calibration.tif* <br> 
![](refs/scan_calibration_small.jpg "scan_calibration.jpg")

To create the scanner reflection model, print using Relative
Colorimetric Intent then scan *scanner_calibration.tif*.
The scan should be without any corrections or conversions to a colorspace
but just the native scanner's RGB in a tiff format.

**Epson V850 Scanner RAW Settings**<br>
![](refs/EpsonV850_no_icm.jpg "EpsonV850 no icm.jpg")

Scan at 200 DPI for best results. Provided is a scan from an Epson V850.

    scanner_refl_fix -c scan_calibration_glossy.tif

Makes the scanner_cal.txt model which will be used for future scan corrections and prints some statistical info:

    Reading scanner reflection calibration file and creating "scanner_cal.txt"
    Gammas: 1.52 1.59 1.63 1.69
    Uncorrected reflection rms err 11.04378
    Corrected reflection rms err 0.95490

### Correcting scans
Now you are ready to correct new scans. The simplest way to use *scanner_refl_fix* where
the first argument is the scan to be reflection corrected
and the second argument is the file name for corrected scan.
Alternately the option "-B" will fix one or more tif files and automatically postpend "_f"
to the main tif file name.

### Checking how well scans are corrected for reflection

To test the effectiveness of the reflected light removal, a special chart, *reflection.tif*,
is provided with Red, Green, Blue, Cyan, Yellow, Magenta
and Neutral (white/gray) patches surrounded by white, a middle gray, and black.

&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;**reflection.tif**-Used for verification<br>
![](refs/reflection_small.jpg "reflection.jpg")


To test your scanner, print the chart *reflection.tif* using Adobe's ACPU or other program that can print w/o color management,
then scan it. Then run the following command to remove the reflected light.

    scanner_refl_fix scan_reflection.tif scan_reflection_f.tif

The program can then be used to produce average measurements of the scanner's
RGB values of each patch along with the max
RGB differences (second number labeled 'd' adjacent to the R, G, B values) 
that occur from white background scanner reflections.
The black background produces little reflected light.
the "d" value is the difference between R, G, or B values between the white background and black background.
For example, taking the White patch's red channel, the average is 232.2 and the "d" value is 24.3.
The White patch's "r" channel measures about 246 in the white surround and 222 in the black surround.

Here's the command and results using the scanned, uncorrected tif image:

    scanner_refl_fix -s scan_reflection.tif

    Analyzing Reflection Chart RGB Variations
    File:scan_reflection.tif ,Rows:35, Cols:29

    Neutrals
       White  d    Gray 9  d    Gray 8  d    Gray 7  d    Gray 6  d    Gray 5  d
    R  232.4 24.3   199.5 21.8   165.3 19.3   141.0 16.4   116.1 14.3    93.2 12.1
    G  236.1 24.5   199.7 22.9   167.1 18.4   142.4 17.0   116.4 14.7    92.9 12.1
    B  241.3 24.1   202.8 22.6   167.7 19.4   140.5 16.6   114.0 14.3    91.3 12.0

    Colors
         Red  d     Green  d      Blue  d      Cyan  d    Yellow  d   Magenta  d
    R  173.4 16.7    34.7  5.8    66.4  7.8    35.2  5.2   224.3 25.9   171.2 18.4
    G   34.3  5.2    97.6 11.6    45.2  5.5   123.4 14.1   210.5 24.7    45.7  5.7
    B   20.5  4.0    52.5  7.3   144.8 15.2   208.7 23.2    68.4  8.6    94.2 10.5
    Average of all channel stds:  7.66
    Execution Time: 0.951874

And here's the command and results from the reflection corrected tif image:

    scanner_refl_fix -s scan_reflection_f.tif

    Analyzing Reflection Chart RGB Variations
    File:scan_reflection_f.tif ,Rows:35, Cols:29

    Neutrals
       White  d    Gray 9  d    Gray 8  d    Gray 7  d    Gray 6  d    Gray 5  d
    R  229.5  1.6   197.8  1.0   164.6  2.4   140.9  1.6   116.3  2.4    93.6  2.8
    G  232.7  2.8   197.8  0.8   166.3  1.6   142.0  1.8   116.5  2.4    93.1  2.6
    B  237.5  4.7   200.6  1.9   166.6  1.1   140.0  1.5   113.9  2.1    91.5  2.4

    Colors
         Red  d     Green  d      Blue  d      Cyan  d    Yellow  d   Magenta  d
    R  172.7  1.7    34.9  2.3    66.7  1.7    35.4  1.6   221.7  0.5   170.5  0.6
    G   34.5  1.6    97.8  1.3    45.5  1.0   123.3  0.8   208.4  0.5    46.0  1.0
    B   20.6  2.0    52.7  1.6   144.2  2.1   206.3  1.5    68.6  1.1    94.3  0.4
    Average of all channel stds:  0.89
    Execution Time: 0.924931


Note that the average standard deviations of the RGB channels is reduced from 7.66 to .89

## Creating a Scanner ICC Profile using Argyll and I1Profiler

*scanner_refl_fix* can also be used to create high quality ICC scanner profiles.
This requires XRite's I1Profiler
and a uV cut (M2) spectrophotometer but does not require XRite licenses as it only uses I1Profiler to extract
profile measurement data. Provided are a CGATs RGB file and Tif images of the corresponding targets.
It is strictly not necessary to use the supplied targets. I1Profiler can create arbitrary,
multi-page charts that are sized appropriately for spectrophotometers such as the i1Pro 2 or i1iSis.
However, the provided charts should be easier to use and results can be compared with the
provided samples for both the i1Pro2 and i1iSis spectrophotometers.

### Using the i1iSis Spectrophotometer
For XRite's i1iSis, a 3 page USA Letter format is recommended.
Included are CGATs RGB files and printable images of the 2871 patches over 3 pages.
The first two pages are color patches and the third page is devoted to near neutrals where
color perception is very sensitive to small hue shifts.
The printed tif images can be scanned then spectrophotometer measured for Lab values with the i1iSis.
First open i1Profiler then load the RGB CGATs file into the Patches tab.
Next select USA letter size and profile orientation. The defaults will produce
3, 957 patch images totaling 2871 patches. Measure with M2 (no uV) since scanners have no significant uV.
Check to make sure they match the printed/scanned images.
Samples from my printer/scanner are provided for comparison as well as a batch command
file *makei1isis.bat* to execute these command on the sample files.

First, the 3 charts are printed w/o color management using Adobe's ACPU utility or other appropriate program.
Second, I1Profiler loads the associated RGB CGATs file
and scans the charts saving the resulting measurement file in CGATs format.

Next we generate the required Argyll CGATs file that contains scanner RGB values and
corresponding LAB values from the I1Profilerthen execute Argyll commands to create the profile.
The "-M" option then reads the patch RGB values from patches
from 1 or more tif files (*scanner3pgcg1-3.tif*) then adds the LAB values
from the CGATs spectro measurement file (*scanner3pgcg_M2.txt*) and finally creates
a CGATs file with RGB and associated LAB values .

    scanner_refl_fix -M scanner3pgcg1.tif scanner3pgcg2.tif scanner3pgcg3.tif scanner3pgcg_M2.txt scanner3pgcg_f.txt

Then we run a Windows script file on the original and corrected scan RGBLAB CGATs files to create ICC profiles:

    makescanner scanner3pgcg

and

    makescanner scanner3pgcg_f

These create ICC input profiles named: *scanner3pgcg.icm* and *scanner3pgcg_f.icm*.
This script file (*makescanner.bat*) has a few entries that imports the CGATs RGBLAB file for Argyll
to create a scanner input profile.
The first line changes the Argyll algorithm to conform with ICC's definition of Abs. Col. intent.
The second line processes the CGATs file so it is interpreted as an input data set.
The third line generates the profile. Setting the `-r' option to .2 works quite well with the i1iSis
spectrophotometer but I would suggest .5 for the i1Pro 2. which isn't quite as consistent as the iSis.

    set ARGYLL_CREATE_WRONG_VON_KRIES_OUTPUT_CLASS_REL_WP=1
    txt2ti3 -i -v %1.txt %1
    colprof -r .2 -v -qh -ua -D %1.icm -O %1.icm %1

Argyll's created profile completes and shows the following results:

    Profile check complete, peak err = 2.870997, avg err = 0.612049

This creates scanner.txt which is a simple CGATs file with scanner RGBs and measured Lab values. Running this through
Argyll's programs produces an input ICC profile which can be assigned to the scanned images. The same process can be
applied to reflection corrected images. The ICC profile is created from corrected images like so

    scanner_refl_fix -B scanner3pg1cg.tif scanner3pg2cg.tif scanner3pg3cg.tif
    scanner_refl_fix -M scanner3pg1cg.tif scanner3pg2cg.tif scanner3pg3cg.tif scanner3pgcg_M2.txt scanner3pgcg.txt
    scanner_refl_fix -M scanner3pg1cg_f.tif scanner3pg2cg_f.tif scanner3pg3cg_f.tif scanner3pgcg_M2.txt scanner3pgcg_f.txt

Which produces the following outputs:

    Processing image patch files
    File:i1isis/scanner3pg1cg.tif ,Rows:33, Cols:29
    File:i1isis/scanner3pg2cg.tif ,Rows:33, Cols:29
    File:i1isis/scanner3pg3cg.tif ,Rows:33, Cols:29
    and associated CGATs measurement text file: i1isis/scanner3pgcg_M2.txt
    Output RGBLAB CGATs file:i1isis/scanner3pgcg.txt, Patches:2871
    High: Scanner RGB:  225.4 228.2 234.1   Lab:   94.70  -1.48  -1.94
     Low: Scanner RGB:    8.9  10.3  10.0   Lab:    1.89   0.04  -0.40

    Processing image patch files
    File:i1isis/scanner3pg1cg_f.tif ,Rows:33, Cols:29
    File:i1isis/scanner3pg2cg_f.tif ,Rows:33, Cols:29
    File:i1isis/scanner3pg3cg_f.tif ,Rows:33, Cols:29
    and associated CGATs measurement text file: i1isis/scanner3pgcg_M2.txt
    Output RGBLAB CGATs file:i1isis/scanner3pgcg_f.txt, Patches:2871
    High: Scanner RGB:  227.8 231.7 237.6   Lab:   94.70  -1.48  -1.94
     Low: Scanner RGB:    9.1  10.7  10.3   Lab:    1.89   0.04  -0.40

Argyll's created profile from the 1914 patches completes and shows the following results:

    Profile check complete, peak err = 2.432862, avg err = 0.339278

The command file *makescannerargyll.bat* is quite simple invoking several Argyll commands:

**makescannerargyll.bat**

    set ARGYLL_CREATE_WRONG_VON_KRIES_OUTPUT_CLASS_REL_WP=1
    txt2ti3 -i -v %1.txt %1
    colprof -r .2 -v -qh -ua -D %1.icm -O %1.icm %1

### Using the i1Pro 2 Spectrophotometer

See *Using the i1iSis Spectrophotometer* above for details that apply generally to spectrophotometers

For XRite's i1Pro2 a 1 page USA Letter format is supplied.
Included are CGATs RGB files and printable image of 522 patches in landscape.
The printed tif image can be scanned then spectrophotometer measured for Lab values with the i1Pro2.
First open i1Profiler then load the RGB CGATs file into the Patches tab.
Next select USA letter size and landscape orientation. The defaults will produce
a522 patch image. Measure with M2 (no uV) since scanners have no significant uV.
Check to make sure they match the printed/scanned images.
Samples from my printer/scanner are provided for comparison.

I1Pro2 charts are printed in landscape but scanned in profile to fit in the scanner.
This requires that the top of the landscape scan be on the left.
The "-ML" option then reads the patch RGB values left to right starting from the bottom.
The following command reads the sample patches from a reflection corrected
landscape print scanned in profile with the header on the left.

    scanner_refl_fix -ML chart-522_f.tif Pro1000 CG Chart 522 Patches_M2.txt scanner-522_f.txt

Then we run the following Windows script file, a small Windows script file, to
create the ICC profile *scanner_522.icm*.

    makescannerargyll scanner-522_f



## Usage
*scanner_refl_fix* is written in C++17 and uses standard C++ libraries with the exception of the
widely available libtiff and its dependents. This repo uses Windows x64 with libtiff included through vcpkg.
While not tested on other OS's it is standard C++ and should port easily to Linux and Apple OS.
Executing *scanner_refl_fix* shows various usages:

    Version 2.0:
    Usage: scannerreflfix                  [zero or more options] infile.tif outfile.tif
      -A                                   Correct Image Already in Adobe RGB
      -B tif file list                     Batch mode, auto renaming with _f
      -M[L] charts... measurefile outfile  Make CGATS, chart1.tif ... CGATSmeasure.txt and save CGATS.txt
      -M[L] charts... outfile              Scan patch charts and save CGATs file
      -P profile                           Attach profile <profile.icc> to image
      -S edge_refl                         ave refl outside of scanned area (0 to 1, default: .85)
      -s reflection.tif                    Calculate statistics on colors with white, gray and black surrounds
      -W                                   Maximize white (Like Relative Col with tint retention)

                                           Advanced and Test options
      -b batch_file                        text file with list of command lines to execute
      -C calibration_file                  Use scanner reflection calibration text file.
      -c scanner_cal.tif  [Y values]       Create scanner calibration file from reference scan.

      -F 8|16                              Force 8 or 16 bit tif output]
      -I                                   Save intermediate files
      -N gain                              Restore gain (default half of refl matrix gain)
      -R                                   Simulated scanner by adding reflected light
      -T                                   Show line numbers and accumulated time.
      -Z                                   Average multiple input files with No Refl. Correction.
    scannerreflfix.exe models and removes re-reflected light from an area
    approx 1" around scanned RGB values for the document scanners.

#### Most common commands

    scanner_refl_fix -c scanner_cal.tif

This is the essential, first use of *scanner_refl_fix*, as it generates the file *scanner_cal.txt*
which contains the scanner's matrix and gamma. Once created it just needs to be in the same directory
that *scanner_refl_fix* is executed from. It works for all paper types but has to be created for each
physical scanner. See earlier description of how to print and scan the calibration file.

    scanner_refl_fix image.tif image_f.tif

Fix the scanner raw tiff file image *image.tif* and save as *image.tif*
The program corrects reflection error using the file *scanner_cal.txt* which is searched for in the
current directory or, if not there, in the same directory as scanenr_refl_fix.exe.


Fix one or more scanner raw tiff file images and save using the original names with *_f* postpended.
This is most useful for processing lots of RAW scanned images. For instance

    scanner_refl_fix -B image1.tif image2.tif ....imageN.tif
    scanner_refl_fix -B image1.tif

will produce the same result as 

    scanner_refl_fix image1.tif image1_f.tif
    scanner_refl_fix image2.tif image2_f.tif
    etc

A useful command is combining this with the "-P" option which will attach an ICC profile
to the corrected image(s)

    scanner_refl_fix -P scannerprofile.icm -B image1.tif image2.tif ....imageN.tif

The following command can be used to create high quality ICC profiles by combining the RGB values of patches
with the associated measured CGATs file that contains LAB values. See description of this process earlier.

    scanner_refl_fix -M[L] charts... measurefile outfile

Other options that may be useful.

    -P profile

This can be used to automatically attach an ICC profile to the corrected image file otherwise
one would have to attach it in Photoshop or other image tool. I usually use this then convert the
image to a standard colorspace such as sRGB, Adobe RGB, or ProPhoto RGB. The latter two may be
better depending on whether the scanned image has colors exceeding sRGB.

    -S edge_refl

This is great when the surrounding area of the image isn't white. An example is scanning a stamp
where the scanner lid is open and the stamp is placed directly on the glass. A scan of the stamp that is
trimmed to right around the edges needs to be processed using the *-S 0* to prevent darkening the resulting image.
Alternately, scan the stamp with 1" margins around stamp edges and don't use the *`S* option.

    -W

This option is handy to simulate conversion where the resulting image is designed to viewed where white
is RGB 255,255,255. It's kind of similar to Relative Colorimetric except that it retains the tint of
the image. This is desired since the tint of a scanned image should be retained. Useful for
putting a scanned image on the Web. However, this is only useful if the scanned image has areas of white or near white.
For instance if you scan an 18% gray card with no white borders and use the "-W" option it will
turn the card image into a white card.

For best results when reproducing an original this option should
not be used. Instead use Absolute Colorimetric to print the reflection corrected image.

## Installation

The C++ source code for the program and a Solution file for Visual Studio 2019 is included.
The source code uses ISO standard C++17 code. It requires the TIFFLIB library which can
be installed using the Visual Studio vcpkg which also includes libs for Linux and iOS cross-compiling.
This has been tested on Windows 10 and both x86 and x64. The latter executes somewhat faster.

along with
the sample images, scans and spectrophotometer measurement files from my printer, a Canon Pro1000.
Spectrophotometers used were an i1iSis XL2, and I1Pro 2 to generate CGATs RGB/LAB files
for use with Argyll's free software which Graeme Gill has so generously contributed
to the color management community.

The V850 scanner input ICC profiles for both reflection fixed, and unfixed using the 2871 patch
set with the i1iSis and the 522 patch set for the i1Pro 2 are also included.

## Performance

To test the performance a chart was made with the 24 colors measured from an X-Rite Colorchecker.
These were organized using black, white, yellow, red, green, and blue surrounds.
Also each color, duplicates 10x, was randomly placed along the top and right side.

<img src="refs/cc.jpg" width="200" title="CC.jpg">

These were printed using Abs. Col. and measured with the i1iSis. They were also scanned
and tested with and without reflection correction using a range of profile patch counts.
The following is the result with a very high OBA glossy
(same as the paper used to calibrate and profile the scanner):

    Description                  6 CCs  6 CCs Neutrals   Scattered   Scattered Neutrals
             2871 Patches         2.59       2.69            1.02          0.93
             1729 Patches         2.64       2.80            1.11          1.06
             1000 Patches         2.64       2.72            1.14          0.98
              729 Patches         2.68       2.74            1.22          1.03
              512 Patches         2.71       2.76            1.27          1.07
              343 Patches         2.78       2.80            1.41          1.12
              216 Patches         2.94       2.97            1.67          1.41
              125 Patches         3.45       3.45            2.33          2.04
              XRite IT8           4.52       3.93            3.92          3.04

      Fixed, 2871 Patches         0.55       0.46            0.42          0.35
      Fixed, 1729 Patches         0.58       0.53            0.48          0.45
      Fixed, 1000 Patches         0.69       0.59            0.62          0.52
      Fixed,  729 Patches         0.74       0.66            0.66          0.62
      Fixed,  512 Patches         0.84       0.70            0.78          0.67
      Fixed,  343 Patches         1.01       0.75            0.96          0.70
      Fixed,  216 Patches         1.23       1.04            1.19          1.01
      Fixed,  125 Patches         2.02       1.74            1.97          1.70
      Fixed,  XRite IT8           3.48       2.84            3.47          2.84

Note that the XRite IT8 results are from a batch of chem. photo targets and not individually measured.
They also differ spectrally from CMYK and inkjet printers and much of the higher error is metameric failure.
