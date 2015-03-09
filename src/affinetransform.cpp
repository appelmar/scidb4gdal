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

#include "affinetransform.h"


#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <sstream>
#include <vector>
#include <iomanip>
#include <limits>
#include "utils.h"


namespace scidb4gdal
{


    using namespace std;



    AffineTransform::AffineTransform() : _x0 ( 0 ),  _y0 ( 0 ),  _a11 ( 1 ), _a22 ( 1 ), _a12 ( 0 ), _a21 ( 0 ) {}
    AffineTransform::AffineTransform ( double x0, double y0 ) : _x0 ( x0 ),  _y0 ( y0 ),  _a11 ( 1 ), _a22 ( 1 ), _a12 ( 0 ), _a21 ( 0 ) {}
    AffineTransform::AffineTransform ( double x0, double y0, double a11, double a22 ) : _x0 ( x0 ),  _y0 ( y0 ),  _a11 ( a11 ), _a22 ( a22 ), _a12 ( 0 ), _a21 ( 0 ) {}
    AffineTransform::AffineTransform ( double x0, double y0, double a11, double a22, double a12, double a21 )  : _x0 ( x0 ),  _y0 ( y0 ), _a11 ( a11 ),  _a22 ( a22 ), _a12 ( a12 ), _a21 ( a21 )   {}
    AffineTransform::AffineTransform ( const string &astr ) : _x0 ( 0 ),  _y0 ( 0 ),  _a11 ( 1 ), _a22 ( 1 ), _a12 ( 0 ), _a21 ( 0 )
    {
        vector<string> parts;
        boost::split ( parts, astr, boost::is_any_of ( ",; " ) );
        for ( vector<string>::iterator it = parts.begin(); it != parts.end(); ++it ) {
            vector<string> kv;
            boost::split ( kv, *it, boost::is_any_of ( "=:" ) );
            if ( kv.size() != 2 ) {
                Utils::warn ( "Unreadable affine transformation string '" + *it + "' will be ignored" );
            }
            else {
                if ( kv[0].compare ( "x0" ) == 0 ) _x0 = boost::lexical_cast<double> ( kv[1] );
                else if ( kv[0].compare ( "y0" ) == 0 ) _y0 = boost::lexical_cast<double> ( kv[1] );
                else if ( kv[0].compare ( "a11" ) == 0 ) _a11 = boost::lexical_cast<double> ( kv[1] );
                else if ( kv[0].compare ( "a22" ) == 0 ) _a22 = boost::lexical_cast<double> ( kv[1] );
                else if ( kv[0].compare ( "a12" ) == 0 ) _a12 = boost::lexical_cast<double> ( kv[1] );
                else if ( kv[0].compare ( "a21" ) == 0 ) _a21 = boost::lexical_cast<double> ( kv[1] );
                else {
                    Utils::warn ( "Unkown affine transformation parameter '" + kv[0] + "' will be ignored" );
                }
            }
        }
    }




    string AffineTransform::toString()
    {
        stringstream sstr;
        sstr << sstr << setprecision ( numeric_limits< double >::digits10 )
             << "x0" << "=" << _x0  << " "
             << "y0"  << "=" << _y0  << " "
             << "a11" << "=" << _a11 << " "
             << "a22" << "=" << _a22 << " "
             << "a12" << "=" << _a12 << " "
             << "a21" << "=" << _a21;
        return sstr.str();
    }


    bool AffineTransform::isIdentity()
    {
        return ( _a11 == 1 && _a12 == 0 && _a21 == 0 && _a22 == 1 && _x0 == 0 && _y0 == 0 );
    }




}


