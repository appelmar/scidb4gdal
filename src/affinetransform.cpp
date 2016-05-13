/*
Copyright (c) 2016 Marius Appel <marius.appel@uni-muenster.de>

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

namespace scidb4gdal {

using namespace std;

AffineTransform::AffineTransform()
    : _x0(0), _y0(0), _a11(1), _a22(1), _a12(0), _a21(0), _inv(NULL) {}
AffineTransform::AffineTransform(double x0, double y0)
    : _x0(x0), _y0(y0), _a11(1), _a22(1), _a12(0), _a21(0), _inv(NULL) {}
AffineTransform::AffineTransform(double x0, double y0, double a11, double a22)
    : _x0(x0), _y0(y0), _a11(a11), _a22(a22), _a12(0), _a21(0), _inv(NULL) {}
AffineTransform::AffineTransform(double x0, double y0, double a11, double a22,
                                 double a12, double a21)
    : _x0(x0),
      _y0(y0),
      _a11(a11),
      _a22(a22),
      _a12(a12),
      _a21(a21),
      _inv(NULL) {}
AffineTransform::AffineTransform(const string& astr)
    : _x0(0), _y0(0), _a11(1), _a22(1), _a12(0), _a21(0), _inv(NULL) {
  vector<string> parts;
  boost::split(parts, astr, boost::is_any_of(",; "));
  for (vector<string>::iterator it = parts.begin(); it != parts.end(); ++it) {
    vector<string> kv;
    boost::split(kv, *it, boost::is_any_of("=:"));
    if (kv.size() != 2) {
      stringstream serr;
      serr << "Cannot read affine transformation string '" << astr << "'";
      Utils::error(serr.str());
      break;
    } else {
      if (kv[0].compare("x0") == 0)
        _x0 = boost::lexical_cast<double>(kv[1]);
      else if (kv[0].compare("y0") == 0)
        _y0 = boost::lexical_cast<double>(kv[1]);
      else if (kv[0].compare("a11") == 0)
        _a11 = boost::lexical_cast<double>(kv[1]);
      else if (kv[0].compare("a22") == 0)
        _a22 = boost::lexical_cast<double>(kv[1]);
      else if (kv[0].compare("a12") == 0)
        _a12 = boost::lexical_cast<double>(kv[1]);
      else if (kv[0].compare("a21") == 0)
        _a21 = boost::lexical_cast<double>(kv[1]);
      else {
        stringstream serr;
        serr << "Unknown affine transformation parameter '" + kv[0] +
                    "' will be ignored ";
        Utils::warn(serr.str());
      }
    }
  }
}

AffineTransform::~AffineTransform() {
  if (_inv) {
    _inv->_inv = NULL;  // No recursive deletes!!!
    delete _inv;
    _inv = NULL;
  }
}

string AffineTransform::toString() {
  stringstream sstr;
  sstr << setprecision(numeric_limits<double>::digits10) << "x0"
       << "=" << _x0 << " "
       << "y0"
       << "=" << _y0 << " "
       << "a11"
       << "=" << _a11 << " "
       << "a22"
       << "=" << _a22 << " "
       << "a12"
       << "=" << _a12 << " "
       << "a21"
       << "=" << _a21;
  return sstr.str();
}

bool AffineTransform::isIdentity() {
  return (_a11 == 1 && _a12 == 0 && _a21 == 0 && _a22 == 1 && _x0 == 0 &&
          _y0 == 0);
}

AffineTransform::double2 AffineTransform::f(const double2& v) {
  double2 result;
  result.x = _x0 + _a11 * v.x + _a12 * v.y;
  result.y = _y0 + _a21 * v.x + _a22 * v.y;
  return result;
}

void AffineTransform::f(const AffineTransform::double2& v_in,
                        AffineTransform::double2& v_out) {
  v_out.x = _x0 + _a11 * v_in.x + _a12 * v_in.y;
  v_out.y = _y0 + _a21 * v_in.x + _a22 * v_in.y;
}

void AffineTransform::f(AffineTransform::double2& v) {
  double x = v.x;
  v.x = _x0 + _a11 * v.x + _a12 * v.y;
  v.y = _y0 + _a21 * x + _a22 * v.y;
}

AffineTransform::double2 AffineTransform::fInv(const double2& v) {
  if (_inv == NULL) {
    double d = det();
    if (fabs(d) < DBL_EPSILON) {
      Utils::error("Affine transformation not invertible, det=0");
    }
    double d1 = 1 / d;
    double inv_a11 = d1 * _a22;
    double inv_a12 = d1 * (-_a12);
    double inv_a21 = d1 * (-_a21);
    double inv_a22 = d1 * _a11;
    double inv_x0 = -inv_a11 * _x0 + inv_a12 * _y0;
    double inv_y0 = inv_a21 * _x0 - inv_a22 * _y0;

    _inv =
        new AffineTransform(inv_x0, inv_y0, inv_a11, inv_a22, inv_a12, inv_a21);
    _inv->_inv = this;  // Prevent repreated computations of f, f-1, f, f-1, ...
                        // This is dangerous in destruction...
  }
  return _inv->f(v);
}

void AffineTransform::fInv(const AffineTransform::double2& v_in,
                           AffineTransform::double2& v_out) {
  if (_inv == NULL) {
    double d = det();
    if (fabs(d) < DBL_EPSILON) {
      Utils::error("Affine transformation not invertible, det=0");
    }
    double d1 = 1 / d;
    double inv_a11 = d1 * _a22;
    double inv_a12 = d1 * (-_a12);
    double inv_a21 = d1 * (-_a21);
    double inv_a22 = d1 * _a11;
    double inv_x0 = -inv_a11 * _x0 + inv_a12 * _y0;
    double inv_y0 = inv_a21 * _x0 - inv_a22 * _y0;

    _inv =
        new AffineTransform(inv_x0, inv_y0, inv_a11, inv_a22, inv_a12, inv_a21);
    _inv->_inv = this;  // Prevent repreated computations of f, f-1, f, f-1, ...
                        // This is dangerous in destruction...
  }
  _inv->f(v_in, v_out);
}

void AffineTransform::fInv(AffineTransform::double2& v) {
  if (_inv == NULL) {
    double d = det();
    if (fabs(d) < DBL_EPSILON) {
      Utils::error("Affine transformation not invertible, det=0");
    }
    double d1 = 1 / d;
    double inv_a11 = d1 * _a22;
    double inv_a12 = d1 * (-_a12);
    double inv_a21 = d1 * (-_a21);
    double inv_a22 = d1 * _a11;
    double inv_x0 = -inv_a11 * _x0 + inv_a12 * _y0;
    double inv_y0 = inv_a21 * _x0 - inv_a22 * _y0;

    _inv =
        new AffineTransform(inv_x0, inv_y0, inv_a11, inv_a22, inv_a12, inv_a21);
    _inv->_inv = this;  // Prevent repreated computations of f, f-1, f, f-1, ...
                        // This is dangerous in destruction...
  }

  _inv->f(v);
}

double AffineTransform::det() { return _a11 * _a22 - _a12 * _a21; }
}
