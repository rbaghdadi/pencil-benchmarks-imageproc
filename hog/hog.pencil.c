#include "hog.pencil.h"

#include <math.h>   //sqrt, atan, atan2, ceil, floor
#include <string.h> //memset

#if SIGNED_HOG
static const double BINSIZE_IN_DEGREES = 360.0 / NUMBER_OF_BINS;
#else
static const double BINSIZE_IN_DEGREES = 180.0 / NUMBER_OF_BINS;
#endif

#ifndef M_PI
#define M_PI           3.14159265358979323846
#endif

inline int min(int a, int b) {
    return a <= b ? a : b;
}

inline double mind(double a, double b) {
    return a <= b ? a : b;
}

inline int max(int a, int b) {
    return a > b ? a : b;
}

inline void normalize(double hist[NUMBER_OF_CELLS][NUMBER_OF_CELLS][NUMBER_OF_BINS]) {
    static const double l2_hys_threshold = 0.15;
    double sum = 0.0;
    for (int i = 0; i < NUMBER_OF_CELLS; ++i)
        for (int j = 0; j < NUMBER_OF_CELLS; ++j)
            for (int k = 0; k < NUMBER_OF_BINS; ++k)
                sum += hist[i][j][k] * hist[i][j][k];
    if (sum == 0.0)
        return;
    double scale = 1.0/sqrt(sum);
    sum = 0.0;
    for (int i = 0; i < NUMBER_OF_CELLS; ++i)
        for (int j = 0; j < NUMBER_OF_CELLS; ++j)
            for (int k = 0; k < NUMBER_OF_BINS; ++k) {
                hist[i][j][k] = mind(hist[i][j][k] * scale, l2_hys_threshold);
                sum += hist[i][j][k] * hist[i][j][k];
            }
    for (int i = 0; i < NUMBER_OF_CELLS; ++i)
        for (int j = 0; j < NUMBER_OF_CELLS; ++j)
            for (int k = 0; k < NUMBER_OF_BINS; ++k)
                hist[i][j][k] = hist[i][j][k] * scale;
}

static void hog( const int rows
               , const int cols
               , const int step
               , const uint8_t image[static const restrict rows][step]
               , const double location_x
               , const double location_y
               , const double block_size
               , double hist[NUMBER_OF_CELLS][NUMBER_OF_CELLS][NUMBER_OF_BINS]    //out
               )
{
    const double cell_size = block_size / NUMBER_OF_CELLS;
    const double minx = location_x - block_size / 2.0;
    const double miny = location_y - block_size / 2.0;
    const double maxx = location_x + block_size / 2.0;
    const double maxy = location_y + block_size / 2.0;

    const int minxi = max(ceil(minx), 1);
    const int minyi = max(ceil(miny), 1);
    const int maxxi = min(ceil(maxx), cols - 2);
    const int maxyi = min(ceil(maxy), rows - 2);

    memset(hist, 0, HISTOGRAM_BINS);

#if GAUSSIAN_WEIGHTS
    const float sigma = block_size / 2.0;
    const float sigmaSq = sigma*sigma;
    const float m1p2sigmaSq = -1.0 / (2.0 * sigmaSq);
#endif

#pragma scop
// TODO 1: #pragma pencil independent ?
    for (int pointy = minyi; pointy <= maxyi; ++pointy) {
#if SPARTIAL_WEIGHTS
        const double relative_pos_y = (pointy - miny) / cell_size - 0.5;
        const int cellyi = floor(relative_pos_y);
        const double yscale1 = relative_pos_y - cellyi;
        const double yscale0 = 1.0 - yscale1;
#endif
#if GAUSSIAN_WEIGHTS
        const double dy = pointy - location_y;
        const double dySq = dy*dy;
#endif
// TODO 1: #pragma pencil independent ?
        for (int pointx = minxi; pointx <= maxxi; ++pointx) {
#if SPARTIAL_WEIGHTS
            const double relative_pos_x = (pointx - minx) / cell_size - 0.5;
            const int cellxi = floor(relative_pos_x);
            const double xscale1 = relative_pos_x - cellxi;
            const double xscale0 = 1.0 - xscale1;
#endif
#if GAUSSIAN_WEIGHTS
            const double dx = pointx - location_x;
            const double dxSq = dx*dx;
#endif
            const double mdx = image[pointy][pointx+1] - image[pointy][pointx-1];
            const double mdy = image[pointy+1][pointx] - image[pointy-1][pointx];

            double magnitude = hypot(mdx, mdy);   //or = sqrt(mdx*mdx + mdy*mdy);
#if SIGNED_HOG
            const double orientation = atan2(mdy, mdx) / M_PI * 180.0;
#else
            const double orientation = tan2(mdy / mdx + DBL_EPSILON) / M_PI * 180.0 + 90.0;
#endif
#if GAUSSIAN_WEIGHTS
            magnitude *= exp((dxSq+dySq) * m1p2sigmaSq);
#endif
            const double relative_orientation = (orientation - BINSIZE_IN_DEGREES/2.0) / BINSIZE_IN_DEGREES;
            int bin1 = ceil(relative_orientation);
            int bin0 = bin1 - 1;
            const double bin_weight0 = magnitude * (bin1 - relative_orientation);
            const double bin_weight1 = magnitude * (relative_orientation - bin0);
            bin0 = (bin0 + NUMBER_OF_BINS) % NUMBER_OF_BINS;
            bin1 = (bin1 + NUMBER_OF_BINS) % NUMBER_OF_BINS;

#if SPARTIAL_WEIGHTS
            if (cellyi >= 0 && cellxi >= 0) {
                hist[cellyi][cellxi][bin0] += yscale0 * xscale0 * bin_weight0;
                hist[cellyi][cellxi][bin1] += yscale0 * xscale0 * bin_weight1;
            }
            if (cellyi >= 0 && cellxi < NUMBER_OF_CELLS - 1) {
                hist[cellyi][cellxi+1][bin0] += yscale0 * xscale1 * bin_weight0;
                hist[cellyi][cellxi+1][bin1] += yscale0 * xscale1 * bin_weight1;
            }
            if (cellyi < NUMBER_OF_CELLS - 1 && cellxi >= 0) {
                hist[cellyi+1][cellxi][bin0] += yscale1 * xscale0 * bin_weight0;
                hist[cellyi+1][cellxi][bin1] += yscale1 * xscale0 * bin_weight1;
            }
            if (cellyi < NUMBER_OF_CELLS - 1 && cellxi < NUMBER_OF_CELLS - 1) {
                hist[cellyi+1][cellxi+1][bin0] += yscale1 * xscale1 * bin_weight0;
                hist[cellyi+1][cellxi+1][bin1] += yscale1 * xscale1 * bin_weight1;
            }
#elif NUMBER_OF_CELLS == 1
            hist[0][0][bin0] += bin_weight0;
            hist[0][0][bin1] += bin_weight1;
#else
            int cellxi = floor((pointx - minx) / cell_size);
            int cellyi = floor((pointy - miny) / cell_size);
            assert(cellxi < NUMBER_OF_CELLS);
            assert(cellyi < NUMBER_OF_CELLS);
            assert(cellxi >= 0);
            assert(cellyi >= 0);
            hist[cellxi][cellyi][bin0] += bin_weight0;
            hist[cellxi][cellyi][bin1] += bin_weight1;
#endif
        }
    }
#pragma endscop
    normalize(hist);
}

static void hog_multi( const int rows
                     , const int cols
                     , const int step
                     , const uint8_t image[static const restrict rows][step]
                     , const int num_locations
                     , const double location_x[static const restrict num_locations]
                     , const double location_y[static const restrict num_locations]
                     , const double block_size
                     , double hist[static const restrict num_locations][NUMBER_OF_CELLS][NUMBER_OF_CELLS][NUMBER_OF_BINS]    //out
                     ) {
// TODO 1: #pragma pencil independent ?
    for (int i = 0; i < num_locations; ++i) {
        hog(rows, cols, step, image, location_x[i], location_y[i], block_size, hist);
    }
}

void pencil_hog( const int rows
               , const int cols
               , const int step
               , const uint8_t image[]
               , const int num_locations
               , const double location_x[]
               , const double location_y[]
               , const double block_size
               , double hist[]    //out
               ) {
    hog_multi(rows,cols,step,image,num_locations,location_x,location_y,block_size,hist);
}
