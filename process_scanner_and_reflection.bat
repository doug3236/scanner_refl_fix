rem generate scanner_cal.txt file that models reflections
scanner_refl_fix -c scan_calibration_glossy.tif

rem evaluate uncorrected scanned rgb errors from surrounds
scanner_refl_fix -s scan_reflection.tif

rem Use generated scanner_cal.txt to correct reflections in scan_reflection.tif
scanner_refl_fix scan_reflection.tif scan_reflection_f.tif

rem evaluate corrected scanned rgb errors from surrounds
scanner_refl_fix -s scan_reflection_f.tif
