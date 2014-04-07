// -*- c -*-
// UjoImro, 2013
// Experimental Code for the CARP Project

#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <assert.h>

#include "resize.pencil.h"

/*
A00 -------- A01
 |            |
 |            |
 |  P         |
 |            |
A10 -------- A11


The coordinates of P are [r,c] (row, col). The function returns
bilinear interpolation of the value of P.
 */

#define bilinear( A00, A01, A11, A10, r, c ) ((1-c) * ( (1-r) * A00 + r * A10 ) + c * ( (1-r) * A01 + r * A11 ))

#define sat( val, lo, hi ) ((val >= lo) ? ((val <= hi) ? val : hi) : lo)

static void resize( const int original_rows
                  , const int original_cols
                  , const int original_step
                  , const unsigned char original[static const restrict original_rows][original_step]
                  , const int resampled_rows
                  , const int resampled_cols
                  , const int resampled_step
                  , unsigned char resampled[static const restrict resampled_rows][resampled_step]
                  )
{
#pragma scop
#if __PENCIL__
    __pencil_assume(original_rows  >  0);
    __pencil_assume(original_cols  >  0);
    __pencil_assume(original_step  >= original_cols);
    __pencil_assume(resampled_rows >  0);
    __pencil_assume(resampled_cols >  0);
    __pencil_assume(resampled_step >= resampled_cols);
#endif
    {
        const float o_h = original_rows;
        const float o_w = original_cols;
        const float n_h = resampled_rows;
        const float n_w = resampled_cols;

        #pragma pencil independent
        for ( int n_r = 0; n_r < resampled_rows; n_r++ )
        {
            #pragma pencil independent
            for ( int n_c = 0; n_c < resampled_cols; n_c++ )
            {
                float o_r = ( n_r + 0.5 ) * (o_h) / (n_h) - 0.5;
                float o_c = ( n_c + 0.5 ) * (o_w) / (n_w) - 0.5;

                float r = o_r - floor(o_r);
                float c = o_c - floor(o_c);

                int coord_00_r = sat( floor(o_r), 0, o_h - 1 );
                int coord_00_c = sat( floor(o_c), 0, o_w - 1 );

                int coord_01_r = coord_00_r;
                int coord_01_c = sat( coord_00_c + 1, 0, o_w - 1 );

                int coord_10_r = sat( coord_00_r + 1, 0, o_h - 1 );
                int coord_10_c = coord_00_c;

                int coord_11_r = sat( coord_00_r + 1, 0, o_h - 1 );
                int coord_11_c = sat( coord_00_c + 1, 0, o_w - 1 );

                unsigned char A00 = original[coord_00_r][coord_00_c];
                unsigned char A10 = original[coord_10_r][coord_10_c];
                unsigned char A01 = original[coord_01_r][coord_01_c];
                unsigned char A11 = original[coord_11_r][coord_11_c];

                resampled[n_r][n_c] = bilinear( A00, A01, A11, A10, r, c );
            }
        }
    }
#pragma endscop
}

void pencil_resize_LN( const int original_rows
                     , const int original_cols
                     , const int original_step
                     , const unsigned char original[]
                     , const int resampled_rows
                     , const int resampled_cols
                     , const int resampled_step
                     , unsigned char resampled[]
                     )
{
    resize(original_rows, original_cols, original_step, original, resampled_rows, resampled_cols, resampled_step, resampled );
}
