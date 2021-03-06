/*
 * To colour a .tif
 */

#include <cstdio>
#include <vector>

#include <algorithm>
#include <math.h>
#include <string.h>
#include <string>
#include <sstream>
#include <assert.h>
#include <ctime>

#include "itkImage.h"
#include "itkImageFileReader.h"
#include "itkImageFileWriter.h"
#include <itkDanielssonDistanceMapImageFilter.h>
#include "itkSize.h"
#include "itkThresholdImageFilter.h"
#include "itkBinaryThresholdImageFilter.h"
#include "itkMeanImageFilter.h"

typedef itk::Image<unsigned char,3> ImageType;
typedef itk::Image<int,3> ImageTypeColour;
ImageType::Pointer im;
ImageTypeColour::Pointer imcolour;
int width, height, depth;
unsigned char *p;
int *pcolour;

int main(int argc, char**argv)
{
	FILE *fp;
	char errfile[] = "error.log";
	fp = fopen(errfile,"w");

	if (argc != 3) {
		printf("Usage: colour input_tiff output_tiff\n");
		fprintf(fp,"Usage: colour input_tiff output_tiff\n");
		fprintf(fp,"Submitted command line: argc: %d\n",argc);
		for (int i=0; i<argc; i++) {
			fprintf(fp,"argv: %d: %s\n",i,argv[i]);
		}		fclose(fp);
		return 1;	// Wrong command line
	}

	printf("Input image file: %s\n",argv[1]);
	printf("Output image file: %s\n",argv[2]);
//	printf("SPECIAL CASE to convert .nii to .tif\n");

	typedef itk::ImageFileReader<ImageType> FileReaderType;
	FileReaderType::Pointer reader = FileReaderType::New();

	reader->SetFileName(argv[1]);
	try
	{
		reader->Update();
	}
	catch (itk::ExceptionObject &e)
	{
		std::cout << e << std::endl;
		fprintf(fp,"Read error on input file\n");
		fclose(fp);
		return 2;	// Read error on input file
	}

	im = reader->GetOutput();

	width = im->GetLargestPossibleRegion().GetSize()[0];
	height = im->GetLargestPossibleRegion().GetSize()[1];
	depth = im->GetLargestPossibleRegion().GetSize()[2];
	printf("Image dimensions: width, height, depth: %d %d %d\n",width,height,depth);

	p = (unsigned char *)(im->GetBufferPointer());

	ImageTypeColour::Pointer imcolour = ImageTypeColour::New();
	ImageTypeColour::SizeType imsize; 
	imsize[0] = width;
	imsize[1] = height;
	imsize[2] = depth;
	ImageTypeColour::IndexType imstart; 
	imstart[0] = 0;
	imstart[1] = 0;
	imstart[2] = 0;
	ImageTypeColour::RegionType imregion; 
	imregion.SetSize(imsize);
	imregion.SetIndex(imstart);
	imcolour->SetRegions(imregion);
	imcolour->Allocate();
	pcolour = (int *)(imcolour->GetBufferPointer());

	for (int k=0; k<width*height*depth; k++) {
		if (p[k] == 0) {
			pcolour[k] = 0;
		} else {

		}
	}

	typedef itk::ImageFileWriter<ImageTypeColour> FileWriterType;
	FileWriterType::Pointer writer = FileWriterType::New();
	writer->SetFileName(argv[2]);
	writer->SetInput(imcolour);
	writer->UseCompressionOn();
	try
	{
		writer->Update();
	}
	catch (itk::ExceptionObject &e)
	{
		std::cout << e << std::endl;
		fprintf(fp,"Write error on output file\n");
		fclose(fp);
		return 3;	// Write error on output file
	}
	printf("Created coloured image file: %s\n",argv[2]);

	return 0;
}