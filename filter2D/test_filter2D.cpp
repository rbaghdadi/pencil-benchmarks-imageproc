// Experimental Research Code for the CARP Project
// UjoImro, 2013

#include <chrono>
#include <opencv2/opencv.hpp>
#include <opencv2/ocl/ocl.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#include "opencl.hpp"
#include "utility.hpp"
#include "imgproc_convolve.clh"
#include "filter2D.pencil.h"

namespace {
    inline int divUp(int total, int grain)
    {
        return (total + grain - 1) / grain;
    }
} // unnamed namespace

namespace carp {
    
    cv::ocl::oclMat
    filter2D( carp::opencl::device & device, cv::ocl::oclMat gpu_gray, cv::ocl::oclMat kernel_gpu, int border_type ) {
        CV_Assert(gpu_gray.depth() == CV_32F);
        CV_Assert(kernel_gpu.depth() == CV_32F);
        cv::ocl::oclMat gpu_convolve;        
        gpu_convolve.create(gpu_gray.size(), gpu_gray.type());
        CV_Assert(gpu_gray.type() == kernel_gpu.type() && gpu_gray.size() == gpu_convolve.size());

        std::string kernelName = "convolve_D5";

        CV_Assert(gpu_gray.depth() == CV_32FC1);
        CV_Assert(kernel_gpu.depth() == CV_32F);
        CV_Assert(kernel_gpu.cols <= 17 && kernel_gpu.rows <= 17);

        gpu_convolve.create(gpu_gray.size(), gpu_gray.type());

        CV_Assert(gpu_gray.cols == gpu_convolve.cols && gpu_gray.rows == gpu_convolve.rows);
        CV_Assert(gpu_gray.type() == gpu_convolve.type());

        cv::ocl::Context  *clCxt = gpu_gray.clCxt;
        int channels = gpu_convolve.oclchannels();
        int depth = gpu_convolve.depth();

        size_t vector_length = 1;
        int offset_cols = ((gpu_convolve.offset % gpu_convolve.step) / gpu_convolve.elemSize1()) & (vector_length - 1);
        int cols = divUp(gpu_convolve.cols * channels + offset_cols, vector_length);
        int rows = gpu_convolve.rows;

        std::vector<size_t> localThreads  = { 16, 16, 1 };
        std::vector<size_t> globalThreads = { divUp(cols, localThreads[0]) *localThreads[0],
                                              divUp(rows, localThreads[1]) *localThreads[1],
                                              1 };
                
        device[kernelName] (
            reinterpret_cast<cl_mem>(gpu_gray.data),
            reinterpret_cast<cl_mem>(kernel_gpu.data),
            reinterpret_cast<cl_mem>(gpu_convolve.data),
            gpu_gray.rows,
            cols,
            static_cast<int>(gpu_gray.step),
            static_cast<int>(gpu_convolve.step),
            static_cast<int>(kernel_gpu.step),
            kernel_gpu.rows,
            kernel_gpu.cols
            ).groupsize( localThreads, globalThreads );

        return gpu_convolve;
    } // filter2D
    
} // namespace carp


template<class T0>
void
time_filter2D( carp::opencl::device & device, T0 & pool, int iteration )
{

    double cpu_gpu_quotient=0;
    double pencil_gpu_quotient=0;
    double pencil_cpu_quotient=0;

    int64_t nums = 0;
        
    for ( int q=0; q<iteration; q++ ) {
        
        for ( auto & item : pool ) {
            PRINT(item.path());

            long int elapsed_time_gpu = 0;
            long int elapsed_time_cpu = 0;
            long int elapsed_time_pencil = 0;
                    
            cv::Mat cpu_gray;
            cv::Mat check;    

            cv::cvtColor( item.cpuimg(), cpu_gray, CV_RGB2GRAY );
            cpu_gray.convertTo( cpu_gray, CV_32F, 1.0/255. );
            cv::Mat host_convolve;
            
            float kernel_data[] = {-1, -1, -1
                                   , 0,  0,  0
                                   , 1,  1,  1
            };
            cv::Mat kernel_cpu(3, 3, CV_32F, kernel_data);

            const auto cpu_start = std::chrono::high_resolution_clock::now();
            cv::filter2D( cpu_gray, host_convolve, -1, kernel_cpu, cv::Point(-1,-1), 0.0, cv::BORDER_REPLICATE );
            const auto cpu_end = std::chrono::high_resolution_clock::now();
            elapsed_time_cpu += carp::microseconds(cpu_end - cpu_start);

            const auto gpu_start = std::chrono::high_resolution_clock::now();
            cv::ocl::oclMat gpu_convolve;            
            cv::ocl::oclMat gpu_gray(cpu_gray);
            cv::ocl::oclMat kernel_gpu(kernel_cpu);
            gpu_convolve = carp::filter2D( device, gpu_gray, kernel_gpu, cv::BORDER_REPLICATE );
            check = gpu_convolve;
            const auto gpu_end = std::chrono::high_resolution_clock::now();
            elapsed_time_gpu += carp::microseconds(gpu_end - gpu_start);

            // pencil test:
            cv::Mat pencil_conv(cpu_gray.size(), CV_32F);
            const auto pencil_start = std::chrono::high_resolution_clock::now();
            pencil_filter2D( cpu_gray.rows, cpu_gray.cols, cpu_gray.step1(), cpu_gray.ptr<float>(),
                             kernel_cpu.rows, kernel_cpu.cols, kernel_cpu.step1(), kernel_cpu.ptr<float>(),
                             pencil_conv.step1(), pencil_conv.ptr<float>() );
            const auto pencil_end = std::chrono::high_resolution_clock::now();
            elapsed_time_pencil += carp::microseconds(pencil_end - pencil_start);
            
            // Verifying the results
            if ( (cv::norm(host_convolve - check) > 0.01) ||
                 (cv::norm(pencil_conv - host_convolve) > 0.01) ) {
                PRINT(cv::norm(check - host_convolve));
                PRINT(cv::norm(pencil_conv - host_convolve));

                cv::Mat cpu;
                cv::Mat pencil;
                cv::Mat gpu;
                host_convolve.convertTo( cpu, CV_8U, 255. );
                check.convertTo( gpu, CV_8U, 255. );
                pencil_conv.convertTo( pencil, CV_8U, 255. );

                PRINT(cv::norm(gpu-cpu));
                PRINT(cv::norm(gpu-pencil));
                PRINT(cv::norm(cpu-pencil));
                
                cv::imwrite( "host_convolve.png", cpu );
                cv::imwrite( "gpu_convolve.png", gpu );
                cv::imwrite( "pencil_convolve.png", pencil );
                cv::imwrite( "diff.png", cv::abs(gpu-pencil) );
                
                throw std::runtime_error("The GPU results are not equivalent with the CPU results.");                
            }

            if (elapsed_time_gpu > 1) {
                cpu_gpu_quotient += static_cast<double>(elapsed_time_cpu) / elapsed_time_gpu;
                pencil_gpu_quotient += static_cast<double>(elapsed_time_pencil) / elapsed_time_gpu;
                pencil_cpu_quotient += static_cast<double>(elapsed_time_pencil) / elapsed_time_cpu;
                nums++;
            }
                        
            carp::Timing::print( "convolve image", elapsed_time_cpu, elapsed_time_gpu, elapsed_time_pencil );

        } // for pool
            
    } // for q 

    carp::Timing::CSI( cpu_gpu_quotient, pencil_gpu_quotient, pencil_cpu_quotient, nums );    


    return;
} // text_boxFilter


int main(int argc, char* argv[])
{

    std::cout << "This executable is iterating over all the files which are present in the directory `./pool'. " << std::endl;    

    auto pool = carp::get_pool("pool");

    // Initializing OpenCL
    cv::ocl::Context * context = cv::ocl::Context::getContext();
    carp::Timing::printHeader();
    carp::opencl::device device(context);
    device.source_compile( imgproc_convolve_cl, imgproc_convolve_cl_len,
                           carp::string_vector("convolve_D5" ) );
    time_filter2D( device, pool, 1 );
    return EXIT_SUCCESS;    
} // main


















// LuM end of file