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


#ifndef TEMPORAL_REFERENCE_H
#define TEMPORAL_REFERENCE_H


#include <sstream>
#include <string>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/date_time/gregorian/gregorian.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

namespace scidb4geo
{

    using namespace std;

    // Enumeration to model the granularitiy of temporal references
    enum TResolution {NONE, YEAR, MONTH, WEEK, DAY, HOUR, MINUTE, SECOND, FRACTION};

    // Forward declarations
    class TPoint;
    class TInterval;
    class TReference;
    
    /**
     * Class that represents a point in time with variable resolution
     */
    class TPoint
    {
        friend TPoint operator+ ( const TPoint &l, const TInterval &r );
        friend TPoint operator+ ( const TInterval &l, const TPoint &r );
        friend TPoint operator- ( const TPoint &l, const TInterval &r );
        friend TPoint operator- ( const TInterval &l, const TPoint &r );

        friend class TReference;

    public:
	TPoint ( );
	
        /**
         * Construction from ISO 8601 string
         * @param str ISO 8601 string
         **/
        TPoint ( string str );

        /**
        * ISO 8601 string output
        **/
        string toStringISO();


        boost::posix_time::ptime _pt;

        /**
        * Guessed resolution, i.e. least significant temporal element that is specified in the given string
        **/
        TResolution _resolution;

    protected:
	int _year, _month, _week, _doy, _dow, _dom, _hour, _minute, _second, _fraction;
	int _tz_hour, _tz_minute;

    private:
        void init();
        
    };

    class TInterval
    {

        friend class TReference;

        friend TInterval operator+ ( const TInterval &l, const TInterval &r );
        friend TInterval operator- ( const TInterval &l, const TInterval &r );
        friend TInterval operator* ( const TInterval &l, const int &r );
        friend TInterval operator* ( const int &l, const TInterval &r );


    public:


        /**
         * Default constructor creates a time interval that equals 0.
         **/
        TInterval();

        /**
        * Construction from ISO 8601 string.
        * @param str ISO 8601 string
        **/
        TInterval ( string str );

        /**
        * Duration given as number of months. Months vary in its number of days.
        **/
        int _md;

        /**
        * Duration given as number of years. Years may or may not count the same number of days, but always
        * have 12 months.
        **/
        int _yd;


        /**
        * Duration given as number of days. Days may or may not count the same number of seconds.
        **/
        boost::gregorian::date_duration  _dd;

        /**
        * Exact posix time difference (seconds).
        **/
        boost::posix_time::time_duration _td;

        /**
        * Guessed resolution, most significant user-specified entry of a given string
        **/
        TResolution _resolution;

        /**
        * ISO 8601 string output
        **/
        std::string toStringISO();

    private:
        void init();
    };





    /**
     * Class that stores the temporal reference of an array, i.e. a start point and the cell size.
     * Methods to derive the (integer) dimension value for a given date / time and to get the date / time at a specific index are provided.
    **/
    class TReference
    {	
    public:
      /**
        * Construction of a temporal reference based on a start point and a duration
         * representing the temporal distance between two neighbouring array cells. Based on
         * the given duration, the temporal resolution is to some degree "guessed":
         * Examples:
         * - 1 day always represents a whole day, whether it has 23,24, or 25 hours;
         * - 1 month always represents a whole month and does not convert to days.
         * Combining incompatible date/time values in the cellsize might lead to unpredictable results:
         * (Bad) Examples: P1MT1H (1 month + 1 hour)
         * (Good) Examples: P1M, P1Y6M, P16D, PT1H, PT30M, PT100S
         **/
        TReference ( string t0text, string dttext );
	TReference ();
	~TReference();

        /**
         * Returns the start date/time, i.e. the date/time at dimension index 0
        **/
        TPoint     getStart();

        /**
         * Returns the temporal distance between neighbouring cells
        **/
        TInterval  getCellsize();

        /**
         * Returns the date/time at a given index
        **/
        TPoint     datetimeAtIndex ( int index );

        /**
         * Returns the index at a specific date/time.
         * Cells are assumed as temporal periods (i.e. with a start and end time/date)
         * within which values do not change.
        **/
        int    indexAtDatetime ( TPoint &t );

    protected:
        TPoint *_t0;
        TInterval *_dt;

        TResolution _r;
    };



}

#endif

