/*
scidb4geo - A SciDB plugin for managing spatially referenced arrays
Copyright (C) 2015 Marius Appel <marius.appel@uni-muenster.de>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
-----------------------------------------------------------------------------*/



#include "TemporalReference.h"

#include <sstream>
#include <vector>
#include <iomanip>
#include <limits>
#include <cctype>
#include <cmath>
#include <ctime>


using namespace boost::posix_time;
using namespace boost::gregorian;

namespace scidb4geo
{
    //TODO: Add logger
    using namespace std;



    int countNextDigits ( string &s, int start )
    {
        int i = start;
        int len = s.length();
        while ( i < len && isdigit ( s[i] ) ) ++i;
        return i - start;
    }


    int countNextNumberChars ( string &s, int start )
    {
        int i = start;
        int len = s.length();
        if ( i < len && ( s[i] == '+' || s[i] == '-' ) ) ++i;
        while ( i < len && isdigit ( s[i] ) ) ++i;
        if ( i < len && ( s[i] == '.' || s[i] == ',' ) ) ++i; // TODO: accept , ???
        while ( i < len && isdigit ( s[i] ) ) ++i;
        return i - start;
    }


    void TPoint::init()
    {
        _year = _month = _week = _doy = _dow = _dom = _hour = _minute = _second  = _fraction = -1;
        _tz_hour = _tz_minute = 0;
        _resolution = NONE;
    }
    
    TPoint::TPoint()
    {
	init();
    }

    TPoint::TPoint ( string str )
    {
        init();
        int ndig = 0;
        int pos = 0;
        int len = str.length();

        boost::algorithm::trim ( str );

        while ( pos < len && !isdigit ( str[pos] ) ) ++pos;


        if ( countNextDigits ( str, pos ) < 4 ) {
            // TODO: Error
        }
        _year = boost::lexical_cast<int> ( str.substr ( pos, 4 ) );
        pos += 4;
        if ( len == pos ) {
            // END: Resolution YEAR
            _resolution = YEAR;
        }
        else {
            if ( str[pos] == '-' ) ++pos;
            if ( str[pos] == 'W' ) {
                ++pos;
                if ( countNextDigits ( str, pos ) < 2 ) {
                    // TODO: Error
                }
                _week = boost::lexical_cast<int> ( str.substr ( pos, 2 ) );
                // TODO: Assert week in [01,53]
                pos += 2;
                _resolution = WEEK;
                if ( pos < len ) {
                    if ( str[pos] == '-' ) ++pos;
                    ndig = countNextDigits ( str, pos );
                    if ( ndig != 1 ) {
                        // TODO: Error
                    }
                    _dow = boost::lexical_cast<int> ( str.substr ( pos, 1 ) );
                    // TODO: Assert dow in [1,7]
                    _resolution = DAY;
                    // READY, MAYBE TIME
                    ++pos;
                }

            }
            else {
                ndig = countNextDigits ( str, pos );
                if ( ndig == 3 ) {
                    _doy = boost::lexical_cast<int> ( str.substr ( pos, 3 ) );
                    // TODO: Assert doy in [001,365]
                    pos += 3;
                    _resolution = DAY;
                    // READY, MAYBE TIME

                }
                else if ( ndig == 2 || ndig == 4 ) {
                    _month = boost::lexical_cast<int> ( str.substr ( pos, 2 ) );
                    // TODO: Assert month in [01,12]
                    pos += 2;
                    _resolution = MONTH;
                    if ( pos < len ) {
                        if ( str[pos] == '-' ) ++pos;
                        ndig = countNextDigits ( str, pos );
                        if ( ndig != 2 ) {
                            // TODO: Error
                        }
                        _dom = boost::lexical_cast<int> ( str.substr ( pos, 2 ) );
                        // TODO: Assert dom in [01,31]

                        _resolution = DAY;
                        // READY, MAYBE TIME
                        pos += 2;
                    }

                }

                else {
                    //TODO: Error
                }


            }
        }

        if ( pos < len ) {
            if ( _resolution == DAY ) {
                if ( str[pos] == 'T' || str[pos] == ' ' ) ++pos;
                if ( countNextDigits ( str, pos ) >= 2 ) {
                    _hour = boost::lexical_cast<int> ( str.substr ( pos, 2 ) );
                    pos += 2;
                    _resolution = HOUR;
                    if ( str[pos] == ':' ) ++pos;
                }
                if ( countNextDigits ( str, pos ) >= 2 ) {
                    _minute = boost::lexical_cast<int> ( str.substr ( pos, 2 ) );
                    pos += 2;
                    _resolution = MINUTE;
                    if ( str[pos] == ':' ) ++pos;
                }
                if ( countNextDigits ( str, pos ) >= 2 ) {
                    _second = boost::lexical_cast<int> ( str.substr ( pos, 2 ) );
                    pos += 2;
                    _resolution = SECOND;
                }
                if ( str[pos] == '.' ) {
                    ++pos;
                    ndig = countNextDigits ( str, pos );
                    _fraction = boost::lexical_cast<int> ( str.substr ( pos, ndig ) );
                    pos += ndig;
                    _resolution = FRACTION;
                }

            }
        }


        if ( pos < len ) {
            if ( _resolution > DAY ) {
                // Time zone e.g. +02:30
                if ( str[pos] == 'Z' ) {
                    _tz_hour = 0;
                    _tz_minute = 0;
                }
                else if ( str[pos] == '+' ) {
                    ++pos;
                    if ( countNextDigits ( str, pos ) == 2 ) {
                        _tz_hour = boost::lexical_cast<int> ( str.substr ( pos, 2 ) );
                        _tz_minute = 0;
                        pos += 2;
                        if ( str[pos] == ':' ) ++pos;
                    }
                    if ( countNextDigits ( str, pos ) == 2 ) {
                        _tz_minute = boost::lexical_cast<int> ( str.substr ( pos, 2 ) );
                        pos += 2;
                    }
                }
                else if ( str[pos] == '-' ) {
                    ++pos;
                    if ( countNextDigits ( str, pos ) == 2 ) {
                        _tz_hour = -boost::lexical_cast<int> ( str.substr ( pos, 2 ) );
                        _tz_minute = 0;
                        pos += 2;
                        if ( str[pos] == ':' ) ++pos;
                    }
                    if ( countNextDigits ( str, pos ) == 2 ) {
                        _tz_minute = boost::lexical_cast<int> ( str.substr ( pos, 2 ) );
                        pos += 2;
                    }

                }
            }
        }



        date d;

        // Build posixtime
        if ( _month >= 0 ) {
            if ( _dom < 0 ) _dom = 1;
            d = date ( _year, _month, _dom );
        }
        else if ( _week >= 0 ) {
            if ( _dow < 0 ) d = date ( _year, 1, 1 ) + date_duration ( ( _week - 1 ) * 7 ) ;
            else d = date ( _year, 1, 1 ) + date_duration ( ( _week - 1 ) * 7 + ( _dow - 1 ) ) ;
            _resolution = DAY;

        }
        else if ( _doy >= 0 ) {
            d = date ( _year, 1, 1 ) + date_duration ( _doy - 1 ) ;
        }
        else {
            d = date ( _year, 1, 1 );
        }

        _pt = ptime ( d,
                      hours ( ( _hour >= 0 ) ? _hour : 0 ) +
                      minutes ( ( _minute >= 0 ) ? _minute : 0 ) +
                      seconds ( ( _second >= 0 ) ? _second : 0 ) );



    }





    string TPoint::toStringISO()
    {
        //return to_iso_extended_string(_pt);

        stringstream s;
        s << setw ( 4 ) << std::setfill ( '0' ) << _pt.date().year();
        s << "-" << setw ( 2 ) << std::setfill ( '0' ) << _pt.date().month().as_number();
        s << "-" << setw ( 2 ) << std::setfill ( '0' ) << _pt.date().day().as_number();
        s << "T" << setw ( 2 ) << std::setfill ( '0' ) << _pt.time_of_day().hours();
        s << ":" << setw ( 2 ) << std::setfill ( '0' ) << _pt.time_of_day().minutes();
        s << ":" << setw ( 2 ) << std::setfill ( '0' ) << _pt.time_of_day().seconds();
        s.flush();
        return s.str();

    }



    TPoint operator+ ( const TPoint &l, const TInterval &r )
    {
        TPoint res = l;

        date d = l._pt.date();
        d += years ( r._yd );
        d += months ( r._md );
        d += r._dd;

        time_duration t = l._pt.time_of_day() + r._td;

        res._pt = ptime ( d, t );


        return res;
    }






    TPoint operator+ ( const TInterval &l, const TPoint &r )
    {
        return operator+ ( r, l );
    }


















    void TInterval::init()
    {
        _yd = 0;
        _md = 0;
        _dd = date_duration ( 0 );
        _td = time_duration ( 0, 0, 0 );
        _resolution = NONE;
    }
     
    TInterval::TInterval()
    {
	init();
    }



    TInterval::TInterval ( string str )
    {
        init();
        boost::algorithm::trim ( str );


        vector<float>    numbers;
        vector<char>   symbols;
        int pos = 0;
        int len = str.length();
        int first_time_index = -1;

        if ( pos < len ) {
            if ( str[pos] == 'P' ) ++pos;
        }

        while ( pos < len ) {

            if ( str[pos] == 'T' ) {
                if ( first_time_index >= 0 ) {
                    // ERROR
                    break;
                }
                first_time_index = numbers.size(); //  elements of numvers and symbols with index >= first_time_index correspond to time
                ++pos;

            }


//       int sig = 1;
//       if (str[pos] == '-') {
//  sig = -1;
//  ++pos;
//       }
//       else if (str[pos] == '+')++pos;

            int ndig = countNextNumberChars ( str, pos );
            if ( ndig > 0 ) {
                numbers.push_back ( boost::lexical_cast<float> ( str.substr ( pos, ndig ) ) );
                pos += ndig;

                if ( pos < len ) {
                    symbols.push_back ( str[pos++] );
                }
            } // TODO: Support fractional seconds, add T separator
            else break;
        }





        int l = ( symbols.size() < numbers.size() ) ? symbols.size() : numbers.size();
        if ( l == 0 ) {
            // ERROR
        }
        else {
            for ( int i = 0; i < l; ++i ) {
                switch ( symbols[i] ) {
                case 'Y':
                    _yd += ( int ) numbers[i];
                    break;
                case 'M':
                    if ( i >= first_time_index && first_time_index >= 0 ) // time -> M means minutes
                        _td +=  minutes ( ( int ) numbers[i] );
                    else // Date, M means months
                        _md += ( int ) numbers[i];
                    break;
                case 'W':
                    _dd +=  days ( 7 * ( int ) numbers[i] );
                    break;
                case 'D':
                    _dd +=  days ( ( int ) numbers[i] );
                    break;
                case 'H':
                    _td +=  hours ( ( int ) numbers[i] );
                    break;
                case 'S':
                    _td +=  seconds ( ( int ) numbers[i] );
                    break;
                }
            }
        }

        // Manually convert months and years
        _yd += ( int ) ( _md / 12 );
        _md = ( int ) ( _md % 12 );


        // Derive resolution
        // Find last entry that is not null
        for ( int i = 0; i < l; ++i ) {
            if ( numbers[i] != 0 ) {
                switch ( symbols[i] ) {
                case 'Y':
                    _resolution = YEAR;
                    break;
                case 'M':
                    if ( 0 == first_time_index ) // time -> M means minutes
                        _resolution = MINUTE;
                    else // Date, M means months
                        _resolution = MONTH;
                    break;
                case 'W':
                case 'D':
                    _resolution = DAY;
                    break;
                case 'H':
                    _resolution = HOUR;
                    break;
                case 'S':
                    _resolution = SECOND;
                    break;
                }
                break;
            }


        }

    }




    string TInterval::toStringISO()
    {

        stringstream s;
        s << "P";
        if ( _yd != 0 ) s << _yd << "Y";
        if ( _md != 0 ) s << _md << "M";
        if ( _dd.days() != 0 ) s << _dd.days() << "D";

        if ( _td.hours() != 0 || _td.minutes() != 0 || _td.seconds() != 0 ) { // TODO: Add fractional seconds
            s << "T";
            if ( _td.hours() != 0 ) s << _td.hours()  << "H";
            if ( _td.minutes() != 0 ) s << _td.minutes()   << "M";
            if ( _td.seconds() != 0 ) s << _td.seconds()  << "S";
        }

        s.flush();
        return s.str();

    }




    TInterval operator* ( const int &l, const TInterval &r )
    {
        TInterval res = r;
        res._yd *= l;
        res._md *= l;

        // Manually convert months and years if month >= 12
        res._yd += ( int ) ( res._md / 12 );
        res._md = ( int ) ( res._md % 12 );

        res._dd = date_duration ( res._dd.days() * l );
        res._td *=  l;
        return res;
    }


    TInterval operator* ( const TInterval &l, const int &r )
    {
        return operator* ( r, l );
    }


    TInterval operator+ ( const TInterval &l, const TInterval &r )
    {
        TInterval res = l;
        res._yd += r._yd;
        res._md += r._md;

        // Manually convert months and years if month >= 12
        res._yd += ( int ) ( res._md / 12 );
        res._md = ( int ) ( res._md % 12 );

        res._dd += r._dd;
        res._td +=  r._td;

        res._resolution = ( l._resolution < r._resolution ) ? l._resolution : r._resolution;
        return res;
    }


    // Still unsafe...
    TInterval operator- ( const TInterval &l, const TInterval &r )
    {
        TInterval res = l;
        res._yd -= r._yd;
        res._md -= r._md;

        // Manually convert months and years if month < 0
        if ( res._md <= 0 ) {
            res._md += 12;
            res._yd --;
        }
        res._dd -= r._dd;
        res._td -= r._td;
        res._resolution = ( l._resolution < r._resolution ) ? l._resolution : r._resolution;
        return res;

    }

    
    TReference::TReference ( )
    {
        _t0 = new TPoint ( );
        _dt = new TInterval ( );
        _r = _dt->_resolution;
    }
    
    TReference::TReference ( string t0text, string dttext ) : _t0 ( NULL ), _dt ( NULL )
    {
        _t0 = new TPoint ( t0text );
        _dt = new TInterval ( dttext );
        _r = _dt->_resolution;
    }


    TReference::~TReference( )
    {
        if ( _t0 != NULL ) {
            delete _t0;
        }
        if ( _dt != NULL ) {
            delete _dt;
        }
    }


    TInterval TReference::getCellsize()
    {
        return *_dt;
    }

    TPoint TReference::getStart()
    {
        return *_t0;
    }



    TPoint TReference::datetimeAtIndex ( int index )
    {
        return *_t0 + index * *_dt;
    }



    int TReference::indexAtDatetime ( TPoint &t )
    {
        TInterval dif;

        if ( _r == YEAR ) {
            dif._yd = t._pt.date().year() - _t0->_pt.date().year();
        }
        else if ( _r == MONTH ) {
            dif._yd = t._pt.date().year() - _t0->_pt.date().year();
            dif._md = t._pt.date().month() - _t0->_pt.date().month();
            while ( dif._md < 0 ) {
                dif._md += 12;
                dif._yd -= 1;
            }
            while ( dif._md >= 12 ) {
                dif._md -= 12;
                dif._yd += 1;
            }
        }
        // TODO: Week
        else if ( _r == DAY ) {
            dif._dd = t._pt.date() - _t0->_pt.date();
        }
        else if ( _r >= HOUR ) {
            dif._td = t._pt - _t0->_pt;
        }


        double result;

        double ym;
        if ( ( _dt->_yd * 12 + _dt->_md ) == 0 ) ym = NAN;
        else ym = ( dif._yd * 12 + dif._md ) / ( _dt->_yd * 12 + _dt->_md );
        double d;
        if ( _dt->_dd.days() == 0 ) d = NAN;
        else d = dif._dd.days() / _dt->_dd.days();
        double tt;
        if ( _dt->_td.total_seconds() == 0 ) tt = NAN;
        else tt = dif._td.total_seconds() / _dt->_td.total_seconds(); // TODO: Fractional seconds?

        if ( _r < DAY ) result =  ym;
        else if ( _r == DAY ) result =  d;
        else result =  tt;

        return ( int ) result;

    }














}
