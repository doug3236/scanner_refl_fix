set ARGYLL_CREATE_WRONG_VON_KRIES_OUTPUT_CLASS_REL_WP=1
txt2ti3 -i -v %1.txt %1
colprof -r .5 -v -qh -ua -D %1.icm -O %1.icm %1
