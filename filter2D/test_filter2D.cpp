#include "utility.hpp"
#include "filter2D.pencil.h"

#include <opencv2/core/core.hpp>
#include <opencv2/ocl/ocl.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#include <prl.h>
#include <chrono>

void time_filter2D( const std::vector<carp::record_t>& pool, int iteration )
{
    bool first_execution_opencv = true, first_execution_pencil = true;

    carp::Timing timing("2D filter");

    for ( int q=0; q<iteration; q++ ) {
        for ( auto & item : pool ) {
            cv::Mat cpu_gray;
            cv::cvtColor( item.cpuimg(), cpu_gray, CV_RGB2GRAY );
            cpu_gray.convertTo( cpu_gray, CV_32F, 1.0/255. );

            float kernel_data[] = {-1, -1, -1
                                  , 0,  0,  0
                                  , 1,  1,  1
                                  };
            cv::Mat kernel(3, 3, CV_32F, kernel_data);

            cv::Mat cpu_result, gpu_result, pen_result;
            std::chrono::duration<double> elapsed_time_cpu, elapsed_time_gpu_p_copy;

            {
                const auto cpu_start = std::chrono::high_resolution_clock::now();
                cv::filter2D( cpu_gray, cpu_result, -1, kernel, cv::Point(-1,-1), 0.0, cv::BORDER_REPLICATE );
                const auto cpu_end = std::chrono::high_resolution_clock::now();
                elapsed_time_cpu = cpu_end - cpu_start;
            }
            {
                // Execute the kernel at least once before starting to take time measurements so that the OpenCV kernel gets compiled. The following run is not included in time measurements.
                if (first_execution_opencv)
                {
                    cv::ocl::oclMat gpu_gray(cpu_gray);
                    cv::ocl::oclMat gpu_convolve;
                    cv::ocl::filter2D( gpu_gray, gpu_convolve, -1, kernel, cv::Point(-1, -1), 0.0, cv::BORDER_REPLICATE );
                    first_execution_opencv = false;
                }

                const auto gpu_start_copy = std::chrono::high_resolution_clock::now();
                cv::ocl::oclMat gpu_gray(cpu_gray);
                cv::ocl::oclMat gpu_convolve;
                cv::ocl::filter2D( gpu_gray, gpu_convolve, -1, kernel, cv::Point(-1, -1), 0.0, cv::BORDER_REPLICATE );
                gpu_result = gpu_convolve;
                const auto gpu_end_copy = std::chrono::high_resolution_clock::now();
                elapsed_time_gpu_p_copy = gpu_end_copy - gpu_start_copy;
            }
            {
                // pencil test:
                pen_result = cv::Mat(cpu_gray.size(), CV_32F);

                if (first_execution_pencil)
                {
                    pencil_filter2D( cpu_gray.rows, cpu_gray.cols, cpu_gray.step1(), cpu_gray.ptr<float>(), kernel.rows, kernel.cols, kernel.step1(), kernel.ptr<float>(), pen_result.ptr<float>() );
                    first_execution_pencil = false;
                }

                prl_timings_reset();
                prl_timings_start();
                pencil_filter2D( cpu_gray.rows, cpu_gray.cols, cpu_gray.step1(), cpu_gray.ptr<float>(),
                                 kernel.rows, kernel.cols, kernel.step1(), kernel.ptr<float>(),
                                 pen_result.ptr<float>() );
                prl_timings_stop();
                // Dump execution times for PENCIL code.
                prl_timings_dump();
            }
            // Verifying the results
            if ( (cv::norm(cpu_result - gpu_result) > 0.01) ||
                 (cv::norm(pen_result - cpu_result) > 0.01) )
            {
                cv::Mat cpu;
                cv::Mat pencil;
                cv::Mat gpu;
                cpu_result.convertTo( cpu, CV_8U, 255. );
                gpu_result.convertTo( gpu, CV_8U, 255. );
                pen_result.convertTo( pencil, CV_8U, 255. );

                cv::imwrite( "host_convolve.png", cpu );
                cv::imwrite( "gpu_convolve.png", gpu );
                cv::imwrite( "pencil_convolve.png", pencil );
                cv::imwrite( "diff_convolve.png", cv::abs(gpu-pencil) );

                throw std::runtime_error("The GPU results are not equivalent with the CPU results.");
            }
            // Dump execution times for OpenCV calls.
            timing.print( elapsed_time_cpu, elapsed_time_gpu_p_copy );
        }
    }
}

int main(int argc, char* argv[])
{
    prl_init((prl_init_flags)(PRL_TARGET_DEVICE_DYNAMIC | PRL_PROFILING_ENABLED));

    std::cout << "This executable is iterating over all the files passed to it as an argument. " << std::endl;

    auto pool = carp::get_pool(argc, argv);

#ifdef RUN_ONLY_ONE_EXPERIMENT
    time_filter2D( pool, 1 );
#else
    time_filter2D( pool, 22 );
#endif

    prl_shutdown();
    return EXIT_SUCCESS;
} // main
