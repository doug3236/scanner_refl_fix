rem Read patches from uncorrected scan chart and combine with i1Pro2/i1Profiler measurements
scanner_refl_fix -ML i1pro2/chart-522.tif "i1pro2/Pro1000 CG Chart 522 Patches_M2.txt" i1pro2/scanner_i1pro2.txt

rem Use generated scanner_cal.txt to correct reflections in scan_reflection.tif
scanner_refl_fix i1pro2/chart-522.tif i1pro2/chart-522_f.tif

rem Read patches from uncorrected scan chart and combine with i1Pro2/i1Profiler measurements
scanner_refl_fix -ML i1pro2/chart-522_f.tif "i1pro2/Pro1000 CG Chart 522 Patches_M2.txt" i1pro2/scanner_i1pro2_f.txt
