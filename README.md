Files: CMakeLists.txt
       nitf2jpeg.cpp

Grace Vesom <grace.vesom@gmail.com> (c) 2013

nitf2jpeg converts NITF imagery to JPEG imagery.  Originally, this code was written to rewrite r0 files of the Greene 07 data set: 
 
https://www.sdms.afrl.af.mil/index.php?collection=greene07

The block indices found on the DVDs are corrupted and are recalculated with the following code.  

This code is capable to converting all r-sets to JPEG format on the Greene 07 data set.  It is untested for other nitf files but should work fine, as it just does search/fix for indexing errors.

Compile: 
	 cmake .
	 make	 

Usage:
	 nitf2jpeg <filename_in.pgm.r0> <filename_out.jpg OPTIONAL>

