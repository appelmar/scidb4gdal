/*
Copyright (c) 2015 Marius Appel <marius.appel@uni-muenster.de>

This file is part of scidb4gdal. scidb4gdal is licensed under the MIT license.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
-----------------------------------------------------------------------------*/



#ifndef AFFINE_TRANSFORM_H
#define AFFINE_TRANSFORM_H


#include <sstream>
#include <string>
#include "utils.h"

namespace scidb4gdal
{

    using namespace std;


    /**
     * Affine transformations are used to map array integer coordinates to world SRS coordinates.
     * This class stores six parameters of a 2D affine transformation
     * and provides basic serialization functions.
     * @see GDAL data model specification http://www.gdal.org/gdal_datamodel.html
     */
    class AffineTransform
    {
    public:

        struct double2 {
            double2() : x ( 0 ), y ( 0 ) {}
            double2 ( double x, double y ) {
                this->x = x;
                this->y = y;
            }
            double x;
            double y;
        };

        /**
         * Default constructor, creates an identity transformation
         */
        AffineTransform();

        /**
         * Constructor for translation only
         */
        AffineTransform ( double x0, double y0 );

        /**
         * Constructor for translation and scaling, no rotation, shear
        */
        AffineTransform ( double x0, double y0, double a11, double a22 );

        /**
         * Constructor for specification of all parameters
         */
        AffineTransform ( double x0, double y0, double a11, double a22, double a12, double a21 ) ;

        /**
         * Constructor for parsing string representations
         */
        AffineTransform ( const string &astr );

        /**
         * Default destructor
         */
        ~AffineTransform ( );



        /**
         * Creates a string representation
         */
        string toString();

        /**
         * Transformation parameters, _x0, _y0 represent translation. _a11,_a12,_a21,_a22 desribe the 2x2 transformation matrix
         */
        double _x0, _y0, _a11, _a22, _a12, _a21;


        /**
          * Inverse transformation
          */
        AffineTransform *_inv;


        /**
         * Checks whether an affine transformation is the identity function
         */
        bool isIdentity();

        /**
          * Applies transformation to given pointer
          */
        double2 f ( const double2 &v );

        /**
              * Applies transformation to given pointer
              */
        void f ( const double2 &v_in, double2 &v_out );

        void f ( double2 &v );


        /**
          * Applies inverse transformation to a given point
          */
        double2 fInv ( const double2 &v );

        void fInv ( const double2 &v_in, double2 &v_out );

        void fInv ( double2 &v );

        /**
              * Computes the determinant of the linear transformation matrix (without translation)
              */
        double det();
    };




}

#endif

