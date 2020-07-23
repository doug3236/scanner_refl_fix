rem Read patches from uncorrected scan chart and combine with i1Pro2/i1Profiler measurements
scanner_refl_fix -M i1isis/scanner3pg1cg.tif i1isis/scanner3pg2cg.tif i1isis/scanner3pg3cg.tif i1isis/scanner3pgcg_M2.txt i1isis/scanner3pgcg.txt

rem Use generated scanner_cal.txt to correct reflections in scan_reflection.tif
scanner_refl_fix -B i1isis/scanner3pg1cg.tif i1isis/scanner3pg2cg.tif i1isis/scanner3pg3cg.tif

rem Read patches from uncorrected scan chart and combine with i1Pro2/i1Profiler measurements
scanner_refl_fix -M i1isis/scanner3pg1cg_f.tif i1isis/scanner3pg2cg_f.tif i1isis/scanner3pg3cg_f.tif i1isis/scanner3pgcg_M2.txt i1isis/scanner3pgcg_f.txt
