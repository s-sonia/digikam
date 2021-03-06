/* ============================================================
 *
 * This file is a part of digiKam
 *
 * Date        : 2017-08-08
 * Description : Base functions for dnn module, can be used for face recognition, 
 *               all codes are ported from dlib library (http://dlib.net/)
 *
 * Copyright (C) 2006-2016 by Davis E. King <davis at dlib dot net>
 * Copyright (C) 2017      by Yingjie Liu <yingjiewudi at gmail dot com>
 * Copyright (C) 2017-2018 by Gilles Caulier <caulier dot gilles at gmail dot com>
 *
 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General
 * Public License as published by the Free Software Foundation;
 * either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * ============================================================ */


#ifndef DLIB_BOOST_RANDOM_MERSENNE_TWISTER_HPP
#define DLIB_BOOST_RANDOM_MERSENNE_TWISTER_HPP

#include <iostream>
#include <algorithm>     // std::copy
#include <stdexcept>
#include "uintn.h"
#include "serialize.h"


namespace random_helpers 
{

// ------------------------------------------------------------------------------------

    // http://www.math.keio.ac.jp/matumoto/emt.html
    template<
        class UIntType, 
        int w, 
        int n,
        int m,
        int r,
        UIntType a,
        int u,
        int s,
        UIntType b,
        int t,
        UIntType c,
        int l,
        UIntType val
        >
    class mersenne_twister
    {
    public:
        typedef UIntType result_type;
        const static int word_size = w;
        const static int state_size = n;
        const static int shift_size = m;
        const static int mask_bits = r;
        const static UIntType parameter_a = a;
        const static int output_u = u;
        const static int output_s = s;
        const static UIntType output_b = b;
        const static int output_t = t;
        const static UIntType output_c = c;
        const static int output_l = l;

        const static bool has_fixed_range = false;

        mersenne_twister() { seed(); }

        explicit mersenne_twister(UIntType value) { seed(value); }

        void seed () { seed(UIntType(5489)); }

        // compiler-generated copy ctor and assignment operator are fine

        void seed(UIntType value)
        {
            // New seeding algorithm from 
            // http://www.math.sci.hiroshima-u.ac.jp/~m-mat/MT/MT2002/emt19937ar.html
            // In the previous versions, MSBs of the seed affected only MSBs of the
            // state x[].
            const UIntType mask = ~0u;
            x[0] = value & mask;
            for (i = 1; i < n; i++) {
                // See Knuth "The Art of Computer Programming" Vol. 2, 3rd ed., page 106
                x[i] = (1812433253UL * (x[i-1] ^ (x[i-1] >> (w-2))) + i) & mask;
            }
        }

        result_type min() const { return 0; }
        result_type max() const
        {
            // avoid "left shift count >= with of type" warning
            result_type res = 0;
            for(int i = 0; i < w; ++i)
                res |= (1u << i);
            return res;
        }

        result_type operator()();

        friend void serialize(
            const mersenne_twister& item, 
            std::ostream& out
        )
        {
             serialize(item.x, out);
             serialize(item.i, out);
        }

        friend void deserialize(
            mersenne_twister& item, 
            std::istream& in 
        )
        {
             deserialize(item.x, in);
             deserialize(item.i, in);
        }

    private:

        void twist(int block);

        // state representation: next output is o(x(i))
        //   x[0]  ... x[k] x[k+1] ... x[n-1]     x[n]     ... x[2*n-1]   represents
        //  x(i-k) ... x(i) x(i+1) ... x(i-k+n-1) x(i-k-n) ... x[i(i-k-1)]
        // The goal is to always have x(i-n) ... x(i-1) available for
        // operator== and save/restore.

        UIntType x[2*n]; 
        int i;
    };

// ------------------------------------------------------------------------------------

    template<
        class UIntType, int w, int n, int m, int r, UIntType a, int u,
        int s, UIntType b, int t, UIntType c, int l, UIntType val
        >
    void mersenne_twister<UIntType,w,n,m,r,a,u,s,b,t,c,l,val>::twist(
        int block
    )
    {
        const UIntType upper_mask = (~0u) << r;
        const UIntType lower_mask = ~upper_mask;

        if(block == 0) {
            for(int j = n; j < 2*n; j++) {
                UIntType y = (x[j-n] & upper_mask) | (x[j-(n-1)] & lower_mask);
                x[j] = x[j-(n-m)] ^ (y >> 1) ^ (y&1 ? a : 0);
            }
        } else if (block == 1) {
            // split loop to avoid costly modulo operations
            {  // extra scope for MSVC brokenness w.r.t. for scope
                for(int j = 0; j < n-m; j++) {
                    UIntType y = (x[j+n] & upper_mask) | (x[j+n+1] & lower_mask);
                    x[j] = x[j+n+m] ^ (y >> 1) ^ (y&1 ? a : 0);
                }
            }

            for(int j = n-m; j < n-1; j++) {
                UIntType y = (x[j+n] & upper_mask) | (x[j+n+1] & lower_mask);
                x[j] = x[j-(n-m)] ^ (y >> 1) ^ (y&1 ? a : 0);
            }
            // last iteration
            UIntType y = (x[2*n-1] & upper_mask) | (x[0] & lower_mask);
            x[n-1] = x[m-1] ^ (y >> 1) ^ (y&1 ? a : 0);
            i = 0;
        }
    }

// ------------------------------------------------------------------------------------

    template<
        class UIntType, int w, int n, int m, int r, UIntType a, int u,
        int s, UIntType b, int t, UIntType c, int l, UIntType val
        >
    inline typename mersenne_twister<UIntType,w,n,m,r,a,u,s,b,t,c,l,val>::result_type
    mersenne_twister<UIntType,w,n,m,r,a,u,s,b,t,c,l,val>::operator()(
    )
    {
        if(i == n)
            twist(0);
        else if(i >= 2*n)
            twist(1);
        // Step 4
        UIntType z = x[i];
        ++i;
        z ^= (z >> u);
        z ^= ((z << s) & b);
        z ^= ((z << t) & c);
        z ^= (z >> l);
        return z;
    }

// ------------------------------------------------------------------------------------

} // namespace random


typedef random_helpers::mersenne_twister<uint32,32,351,175,19,0xccab8ee7,11,
7,0x31b6ab00,15,0xffe50000,17, 0xa37d3c92> mt11213b;

// validation by experiment from mt19937.c
typedef random_helpers::mersenne_twister<uint32,32,624,397,31,0x9908b0df,11,
7,0x9d2c5680,15,0xefc60000,18, 3346425566U> mt19937;




#endif // DLIB_BOOST_RANDOM_MERSENNE_TWISTER_HPP

