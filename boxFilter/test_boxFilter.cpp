// UjoImro, 2013

#include <chrono>
#include <opencv2/opencv.hpp>
#include <opencv2/ocl/ocl.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#include "opencl.hpp"
#include "utility.hpp"
#include "filtering_boxFilter.clh"

namespace
{
    inline void normalizeAnchor(int &anchor, int ksize)
    {
        if (anchor < 0)
        {
            anchor = ksize >> 1;
        }

        CV_Assert(0 <= anchor && anchor < ksize);
    }

    inline void normalizeAnchor(cv::Point &anchor, const cv::Size &ksize)
    {
        normalizeAnchor(anchor.x, ksize.width);
        normalizeAnchor(anchor.y, ksize.height);
    }

} // unnamed namespace



template<class T0>
void
time_boxFilter( T0 & pool )
{
    std::vector<int> ksizes = { 11,15,21,25,31,35,41 };
    std::vector<std::pair<std::string, int> > border_types =
        { {"BORDER_REPLICATE",   cv::BORDER_REPLICATE },
          {"BORDER_REFLECT",     cv::BORDER_REFLECT },
          {"BORDER_REFLECT_101", cv::BORDER_REFLECT_101 },
          {"BORDER_CONSTANT",    cv::BORDER_CONSTANT } };

    carp::TimingLong timing;

    for ( auto & item : pool ) {

        PRINT(item.path());
        for ( auto size : ksizes ) {

            PRINT(size);
            std::chrono::microseconds elapsed_time_cpu(0), elapsed_time_gpu(0), elapsed_time_pencil(999999);

            for ( auto & border_type : border_types ) {
                // PRINT(border_type.first);

                cv::Mat cpu_gray;
                cv::cvtColor( item.cpuimg(), cpu_gray, CV_RGB2GRAY );

                cv::Mat cpu_result, gpu_result;

                // the ksize must be 2k+1 form and anchor must be (2k+1)<<1
                cv::Size  ksize(size,size);
                cv::Point anchor((size>>1),(size>>1));


                {
                    auto cpu_start = std::chrono::high_resolution_clock::now();
                    cv::boxFilter( cpu_gray, cpu_result, -1, ksize, anchor, true, border_type.second );
                    auto cpu_end = std::chrono::high_resolution_clock::now();
                    elapsed_time_cpu += (cpu_end - cpu_start);
                }
                {
                    float alpha = ksize.height * ksize.width;
                    size_t blockSizeX = 256, blockSizeY = 1;
                    size_t gSize = blockSizeX - (ksize.width - 1);
                    size_t threads = (gpu_result.offset % gpu_result.step % 4 + gpu_result.cols + 3) / 4;
                    size_t globalSizeX = threads % gSize == 0 ? threads / gSize * blockSizeX : (threads / gSize + 1) * blockSizeX;
                    size_t globalSizeY = ((gpu_result.rows + 1) / 2) % blockSizeY == 0 ? ((gpu_result.rows + 1) / 2) : (((gpu_result.rows + 1) / 2) / blockSizeY + 1) * blockSizeY;

                    normalizeAnchor(anchor, ksize);

                    cv::ocl::Context * context = cv::ocl::Context::getContext();
                    carp::opencl::device device(context);
                    device.source_compile( filtering_boxFilter_cl, filtering_boxFilter_cl_len, carp::make_vector<std::string>("boxFilter_C1_D0")
                                         , " -D anX=" + carp::cast<std::string>(anchor.x)
                                         + " -D anY=" + carp::cast<std::string>(anchor.y)
                                         + " -D ksX=" + carp::cast<std::string>(ksize.width)
                                         + " -D ksY=" + carp::cast<std::string>(ksize.height)
                                         + " -D " + border_type.first
                                         );
                    auto gpu_start = std::chrono::high_resolution_clock::now();
                    cv::ocl::oclMat gpu_filtered( cpu_gray.rows, cpu_gray.cols, CV_8U );
                    cv::ocl::oclMat gpu_gray(cpu_gray);
                    device["boxFilter_C1_D0"]( reinterpret_cast<cl_mem>(gpu_gray.data), reinterpret_cast<cl_mem>(gpu_filtered.data), static_cast<cl_float>(alpha)
                                             , gpu_gray.offset, gpu_gray.wholerows, gpu_gray.wholecols
                                             , static_cast<int>(gpu_gray.step)
                                             , gpu_filtered.offset, gpu_filtered.rows, gpu_filtered.cols, static_cast<int>(gpu_filtered.step)
                                             ).groupsize( { blockSizeX, blockSizeY, 1 }, { globalSizeX, globalSizeY, 1 } );
                    gpu_result = gpu_filtered;
                    auto gpu_end = std::chrono::high_resolution_clock::now();
                    elapsed_time_gpu += (gpu_end - gpu_start);
                    device.erase("boxFilter_C1_D0");
                }
                // Verifying the results
                if ( cv::norm(gpu_result - cpu_result) > 0.01 ) {
                    PRINT(cv::norm(gpu_result - cpu_result));
                    cv::imwrite( "gpu_boxFilter_img.png", gpu_result );
                    cv::imwrite( "cpu_boxFilter_img.png", cpu_result );
                    throw std::runtime_error("The GPU results are not equivalent with the CPU results.");
                }

            } // for border_type

            timing.print( "boxFilter", elapsed_time_cpu, elapsed_time_gpu, elapsed_time_pencil );
        } // for size
    } // for item
} // text_boxFilter


int main(int argc, char* argv[])
{
    std::cout << "This executable is iterating over all the files which are present in the directory `./pool'. " << std::endl;

    auto pool = carp::get_pool("pool");
    time_boxFilter( pool );

    return EXIT_SUCCESS;
} // main
