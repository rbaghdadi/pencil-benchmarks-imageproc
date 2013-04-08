// UjoImro, 2013
// This file tests the OpenCL framework for the CARP project

#include <stdlib.h>
#include <opencv2/core/core.hpp>

#include "opencl.hpp"

const int rows = 10;
const int cols = 15;

int main()
{
    carp::opencl::device device;
    device.compile( {"color.cl"}, {"color"} );
    
    carp::opencl::image<int> cl_image(device, 10, 25);
    
    device["color"]( cl_image.cl(), cl_image.rows(), cl_image.cols() )        
        .groupsize({16,16}, {cl_image.rows(), cl_image.cols()});

    cv::Mat_<int> image = cl_image.get();

    print_image( image, "image" );

    for (int q = 0; q < image.rows; q++ )
        for ( int w = 0; w < image.cols; w++ )
            assert( image(q,w) == q*image.cols + w );
        
    return EXIT_SUCCESS;
}



// LuM end of file