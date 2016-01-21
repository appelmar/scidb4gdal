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
     * @brief Class to store transformation parameter and to perform transformations with that
     * 
     * Affine transformations are used to map array integer coordinates to world SRS coordinates.
     * This class stores six parameters of a 2D affine transformation
     * and provides basic serialization functions.
     * 
     * @see GDAL data model specification http://www.gdal.org/gdal_datamodel.html
     */
    class AffineTransform
    {
    public:
	
	/**
	* @brief A nested structure to represent a 2 dimensional point
	* 
	* This structure represents a spatial point with a x and a y axis.
	*/
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
         * @brief Default constructor, creates an identity transformation
         */
        AffineTransform();
	
	/**
	 * @brief Constructor for translation only
	 * 
	 * @param x0 the x offset
	 * @param y0 the y offset
	 */
	AffineTransform ( double x0, double y0 );

	/**
	 * @brief Constructor for translation and scaling, no rotation, shear
	 * 
	 * @param x0 the x offset
	 * @param y0 the y offset
	 * @param a11 x scaling factor
	 * @param a22 y scaling factor
	 */
	AffineTransform ( double x0, double y0, double a11, double a22 );

	/**
	 * @brief Constructor for specification of all parameters
	 * 
	 * @param x0 the x offset
	 * @param y0 the y offset
	 * @param a11 x scaling factor
	 * @param a22 y scaling factor
	 * @param a12 rotation or shear factor in x
	 * @param a21 rotation or shear factor in y
	 */
	AffineTransform ( double x0, double y0, double a11, double a22, double a12, double a21 ) ;

	/**
	 * @brief Constructor for parsing string representations
	 * 
	 * @param astr Affine Transformation as a string of transformation parameter
	 */
	AffineTransform ( const string &astr );

        /**
         * @brief Default destructor
         */
        ~AffineTransform ( );



        /**
         * @brief Creates a string representation
	 * 
	 * @return string String representation
         */
        string toString();
	
	/** Translationparameter in x */
        double _x0;
	/** Translationparameter in y */
	double _y0;
	/** Scale factor in x dimension */
	double _a11;
	/** Scale factor in y dimension */
	double _a22;
	/** Rotation or shear factor in x */
	double _a12;
	/** Rotation or shear factor in y */
	double _a21;


        /**
          * @brief Inverse transformation
          */
        AffineTransform *_inv;


	/**
	 * @brief Checks whether an affine transformation is the identity function
	 * 
	 * @return bool whether or not this is the identity transformation
	 */
	bool isIdentity();

	/**
	 * @brief Applies transformation to given pointer
	 * 
	 * This function transforms the coordinates of a point into the target reference system by applying
	 * the transformation with the parameters of this class.
	 * 
	 * @param v A two dimensional point representation
	 * 
	 * @return scidb4gdal::AffineTransform::double2 The transformed point with coordinates in the target reference system
	 */
	double2 f ( const double2 &v );

	/**
	 * @brief Applies transformation to given pointer
	 * 
	 * @param v_in 2D point representation in the source reference system
	 * @param v_out 2D point representation as a pointer to the output object
	 * @return void
	 */
	void f ( const double2 &v_in, double2 &v_out );

	/**
	 * @brief Recalculates the coordinates from the given point
	 * 
	 * This function will recacluate the coordinates of the passed point and stores
	 * the new coordinates in this object.
	 * 
	 * @param v 2D point representation (in/out)
	 * @return void
	 */
	
        void f ( double2 &v );

	/**
	 * @brief Applies inverse transformation to a given point
	 * 
	 * Calculates the image coordinates of a given spatial point by applying the inverse matrix 
	 * of this class' parameter.
	 * 
	 * @param v 2D point representation
	 * 
	 * @return scidb4gdal::AffineTransform::double2 The point with image coordinates
	 */
	double2 fInv ( const double2 &v );

	/**
	 * @brief Applies inverse transformation to a given point
	 * 
	 * Calculates the image coordinates of an 2D point by given real world coordinates. The result will
	 * be stored in the second parameter 2D point.
	 * 
	 * @param v_in 2D point with real world coordinates
	 * @param v_out empty 2D point in which the image coordinates are stored.
	 * @return void
	 */
        void fInv ( const double2 &v_in, double2 &v_out );

	
	/**
	 * @brief Applies inverse transformation to a given point
	 * 
	 * Recalculates the coordinates of the a given in real world coordinates into image coordinates by applying
	 * the transformation stated in this class.
	 * 
	 * @param v 2D point representation (in/out)
	 * @return void
	 */
	void fInv ( double2 &v );

	/**
	 * @brief Computes the determinant of the linear transformation matrix (without translation)
	 * 
	 * @return double the determinant
	 */
	double det();
    };




}

#endif

