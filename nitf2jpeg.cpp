/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * nitf2jpeg.cpp
 * Grace Vesom <grace.vesom@gmail.com> (c) 2013
 *
 * nitf2jpeg converts NITF imagery to JPEG imagery.  Originally, 
 * this code was written to rewrite r0 files of the Greene07 
 * data set: 
 * 
 *    https://www.sdms.afrl.af.mil/index.php?collection=greene07
 * 
 * The block indices found on the DVDs are corrupted and are 
 * recalculated with the following code.  
 * 
 * This code is capable to converting all r-sets to JPEG format 
 * on the Greene07 data set.  It is untested for other NITF files 
 * but should work fine, as it just does search/fix for indexing 
 * errors.
 *
 * Usage:
 *   nitf2jpeg <filename_in.pgm.r0> <filename_out.jpg OPTIONAL>  
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <cmath>
#include <limits>
#include <vector>
#include <exception>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>

using namespace std;

#define print(x) cout << #x " = " << x << endl;

std::string tempBlockFilename = "tempBlock256x256.jpg";

struct indexPair{
  unsigned int jpegIndex;
  int blockIndex;
};

void cleanCorruptedIndices(const unsigned char*, vector<indexPair>&, int);
bool hasJpegFlag(const unsigned char*, unsigned int);
void replaceOrErase(vector<indexPair>&, int, unsigned int);
unsigned int searchSubRegion(const unsigned char*, unsigned int, unsigned int);
void findIdx(const char*, int, int, vector<indexPair>&);
void readDataFromBuffer(const char*, unsigned char[], int, int);
int readFromBuffer(const char*, int, int);
unsigned int readBigEndian(const char*, int);
template <class T> 
T readTFromBuffer(const char*, int);

int main ( int argc, char* argv[] ) {

  if ( argc < 2 ) { 
    cerr << "Usage: nitf_rewrite <filename_in.pgm.r0> <filename_out.jpg OPTIONAL>" << endl;  
    return EXIT_FAILURE; 
  }  

  ifstream infile;
  infile.open( argv[1] );
  if ( !infile.is_open() ) { 
    cerr << "File failed to open: " << argv[1] << endl; 
    return EXIT_FAILURE;
  }

  std::string filename_write;
  if ( argc == 2 ) { filename_write.append(argv[1]).append(".jpg"); } 
  else {
    filename_write.append(argv[2]);
    if ( (filename_write.length() < 5) ||
	 !((filename_write.compare(filename_write.length()-4, 4, ".jpg") == 0) ||
	   (filename_write.compare(filename_write.length()-4, 4, ".JPG") == 0)) )
      filename_write.append(".jpg");    
  }
  
  infile.seekg(0, ios::end);
  int length = infile.tellg();  
  if ( length < 1697 ) { // extent of meta-data (ish)
    cerr << "Incorrect filetype, mismatch of metadata: " << argv[1] << endl;
    return EXIT_FAILURE;
  } 
  
  infile.seekg(0, ios::beg);
  char *buffer = new char[length];
  infile.read(buffer, length);
  infile.close();

  int NITFFileHeaderLength = readFromBuffer(buffer, 6, 354); // 6 @ 354
  int LengthOfNthImageSubheader = readFromBuffer(buffer, 6, 363); // 6 @ 363
  int LengthOfNthImage = readFromBuffer(buffer, 10, 369); // 10 @ 369
  int NumBlocksPerRow = readFromBuffer(buffer, 4, 859); // 4 @ 859
  int NumBlocksPerColumn = readFromBuffer(buffer, 4, 863); // 4 @ 863
  int NumberOfPixelsPerBlockHorizontal = readFromBuffer(buffer, 4, 867); // 4 @ 867
  int NumberOfPixelsPerBlockVertical = readFromBuffer(buffer, 4, 871); // 4 @ 871
  int ExtendedSubheaderDataLength = readFromBuffer(buffer, 5, 902); // 5 @ 902
  int startBlockNBandMOffset = 910 + ExtendedSubheaderDataLength - 3 + 4 + 2 * 3;
  int BlockedImageDataOffset = static_cast<int>(readBigEndian(buffer, 910+ExtendedSubheaderDataLength-3));
  int offsetStart = LengthOfNthImageSubheader + NITFFileHeaderLength + BlockedImageDataOffset;

  print(NumBlocksPerRow);
  print(NumBlocksPerColumn);
  print(NumberOfPixelsPerBlockHorizontal);
  print(NumberOfPixelsPerBlockVertical);

  int dataBlockLength = static_cast<int>((length - offsetStart));
  unsigned char *compData = new unsigned char[dataBlockLength];
  readDataFromBuffer(buffer, compData, offsetStart, dataBlockLength);  

  vector<indexPair> idxs;
  findIdx(buffer, startBlockNBandMOffset, NumBlocksPerRow*NumBlocksPerColumn, idxs); 
  cleanCorruptedIndices(compData, idxs, dataBlockLength); // search and fix corrupted indices

  cv::Mat im = cv::Mat::zeros(NumBlocksPerColumn*NumberOfPixelsPerBlockVertical,
			      NumBlocksPerRow*NumberOfPixelsPerBlockHorizontal,
			      CV_8UC1);
  
  for ( int k = 0; k < (idxs.size()-1); ++k ) {

    // copy individual block JPEG stream 
    std::vector<unsigned char> block_(compData + idxs[k].jpegIndex, 
				      compData + idxs[k+1].jpegIndex - 1);      

    // write out JPEG stream
    std::ofstream outfileTest(tempBlockFilename.c_str(), std::ofstream::binary);
    outfileTest.write(reinterpret_cast<const char*>(block_.data()), block_.size());
    outfileTest.close();    

    // decode JPEG into image 
    cv::Mat block = cv::imread(tempBlockFilename.c_str(),0);	  

    // copy image into larger image via block index
    block.copyTo(im(cv::Rect(NumberOfPixelsPerBlockHorizontal*(idxs[k].blockIndex%NumBlocksPerRow),
			     NumberOfPixelsPerBlockVertical*(floor(static_cast<double>(idxs[k].blockIndex)/
    								   static_cast<double>(NumBlocksPerRow))),
       			     NumberOfPixelsPerBlockHorizontal, NumberOfPixelsPerBlockVertical)));
  }
  
  { // decode last block
    int k = idxs.size()-1;
    std::vector<unsigned char> block_(compData + idxs[k].jpegIndex, 
				      compData + dataBlockLength - 1);  
    std::ofstream outfileTest(tempBlockFilename.c_str(), std::ofstream::binary);
    outfileTest.write(reinterpret_cast<const char*>(block_.data()), block_.size());
    outfileTest.close();    
    cv::Mat block = cv::imread(tempBlockFilename.c_str(), 0);
    block.copyTo(im(cv::Rect(NumberOfPixelsPerBlockHorizontal*(idxs[k].blockIndex%NumBlocksPerRow),
    			     NumberOfPixelsPerBlockVertical*(floor(static_cast<double>(idxs[k].blockIndex)/
								   static_cast<double>(NumBlocksPerRow))),
   			     NumberOfPixelsPerBlockHorizontal, NumberOfPixelsPerBlockVertical)));
  }
  
  cv::imwrite(filename_write.c_str(), im);  
  remove(tempBlockFilename.c_str());
  delete [] buffer;
  delete [] compData;
  return 0;
}
  
void cleanCorruptedIndices ( const unsigned char *compData, vector<indexPair> &idxs, int dataBlockLength ) {

  unsigned int newJpgIdx;  

  // find incorrectly written indices
  if ( (idxs[0].jpegIndex > idxs[1].jpegIndex) && (idxs[0].jpegIndex > idxs[2].jpegIndex) ) {
    newJpgIdx = searchSubRegion(compData, 0, idxs[1].jpegIndex);
    replaceOrErase(idxs, 0, newJpgIdx);
  }

  for ( int i = 1; i < (idxs.size()-2); ++i ) {
    if ( idxs[i].jpegIndex > idxs[i+1].jpegIndex ) {
      if ( idxs[i].jpegIndex < idxs[i+2].jpegIndex ) i++;
      newJpgIdx = searchSubRegion(compData, idxs[i-1].jpegIndex, idxs[i+1].jpegIndex);
      replaceOrErase(idxs, i, newJpgIdx);
    }
  }

  if ( idxs[idxs.size()-2].jpegIndex > idxs[idxs.size()-1].jpegIndex ) {
    newJpgIdx = searchSubRegion(compData, idxs[idxs.size()-2].jpegIndex, dataBlockLength);
    replaceOrErase(idxs, idxs.size()-1, newJpgIdx);
  }

  // find indices with incorrect JPEG flags
  if ( !hasJpegFlag(compData, idxs[0].jpegIndex) ) {
    newJpgIdx = searchSubRegion(compData, 0, idxs[1].jpegIndex);
    replaceOrErase(idxs, 0, newJpgIdx);
  }

  for ( int i = 1; i < (idxs.size()-1); ++i ) {
    if ( !hasJpegFlag(compData, idxs[i].jpegIndex) ) {
      newJpgIdx = searchSubRegion(compData, idxs[i-1].jpegIndex, idxs[i+1].jpegIndex);
      replaceOrErase(idxs, i, newJpgIdx);
    }
  }

  if ( !hasJpegFlag(compData, idxs[idxs.size()-1].jpegIndex) ) {
    newJpgIdx = searchSubRegion(compData, idxs[idxs.size()-2].jpegIndex, dataBlockLength);
    replaceOrErase(idxs, idxs.size()-1, newJpgIdx);
  }
}

bool hasJpegFlag ( const unsigned char *compData, unsigned int x ) {
  if ( (compData[x]==255) && (compData[x+1]==216) ) { return true; }
  return false;
}

void replaceOrErase ( vector<indexPair> &idxs, int i, unsigned int newJpgIdx ) {
  if ( newJpgIdx == UINT_MAX ) { idxs.erase(idxs.begin() + i); } 
  else { idxs[i].jpegIndex = newJpgIdx; }
}

unsigned int searchSubRegion ( const unsigned char *compData, unsigned int start, unsigned int end ) {
  unsigned int x = UINT_MAX;
  for( unsigned int i = start + 1; i < end; ++i ) {
    if ((compData[i]==0xFF) && (compData[i+1]==0xD8)) { // JPEG SOI marker
      x = i;
      break;
    }
  }
  return x;
}

void findIdx ( const char *datablock, int start, int numBlocks, vector<indexPair> &idxs ){
  idxs.reserve(numBlocks);
  indexPair idxPair;
  unsigned int x; 
  for( int i = 0; i < numBlocks; ++i ) {
    x = readBigEndian(datablock, start + i*4);
    if ( x < UINT_MAX ) {
      idxPair.jpegIndex = x;
      idxPair.blockIndex = i;
      idxs.push_back(idxPair);
    }
  }
}

void readDataFromBuffer ( const char *buffer, unsigned char datablock[], int start, int length ) {
  for ( int i = 0; i < length; ++i ) {
    datablock[i] = static_cast<unsigned char>(buffer[start++]);
  }
}

int readFromBuffer ( const char *buffer, int numchars, int start ) {
  char* data = new char[numchars]; 
  memcpy(data, buffer + start, numchars);
  int x = atoi(data);
  delete [] data;
  return x; 
}

template <class T>
T readTFromBuffer ( char *buffer, int start ) {
  int sz = sizeof(T);
  char* data = new char[sz];
  memcpy(data, buffer + start, sz);
  T x = *((T*)data);
  delete [] data;
  return x;
}

unsigned int readBigEndian ( const char *buffer, int start ) {
  unsigned int x = 0; 
  for ( int i = 0; i < 4; ++i ) { // big endian
    x += (static_cast<unsigned int>( static_cast<unsigned char>(buffer[start+i]))) << (3-i)*8;
  }
  return x;
}
