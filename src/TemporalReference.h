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

    /**
 * @brief A temporal resolution statement
 *
 * This enumeration refers to the infered temporal resolution that is based on
 *the
 * temporal interval.
 */
    enum TResolution {
        /** no temporal resoultion */
        NONE = 0,
        /** temporal resoultion of one year */
        YEAR = 1,
        /** temporal resolution of one month */
        MONTH = 2,
        /** temporal resoultion of one week */
        WEEK = 3,
        /** temporal resolution of one day */
        DAY = 4,
        /** temporal resolution of one hour */
        HOUR = 5,
        /** temporal resolution of one minute */
        MINUTE = 6,
        /** temporal resolution of one second */
        SECOND = 7,
        /** temporal resolution of the fraction of a second */
        FRACTION = 8
    };

    // Forward declarations
    class TPoint;
    class TInterval;
    class TReference;

    /**
 * @brief Class that represents a point in time with variable resolution
 */
    class TPoint
    {
        friend TPoint operator+(const TPoint& l, const TInterval& r);
        friend TPoint operator+(const TInterval& l, const TPoint& r);
        friend TPoint operator-(const TPoint& l, const TInterval& r);
        friend TPoint operator-(const TInterval& l, const TPoint& r);

        friend class TReference;

      public:
        /**
   * @brief Basic constructor
   *
   */
        TPoint();

        /**
   * @brief Construction from ISO 8601 string
   *
   * The constructor evaluates a given string against a valid ISO 8601 string
   *and creates
   * based on that information this class.
   *
   * @param str ISO 8601 string
   **/
        TPoint(string str);

        /**
  * @brief creates a string representation
  *
  * Creates a string representation of this temporal point that is conform to
  *the ISO 8601 standard.
  *
  * @return string ISO 8601 conform string
  **/
        string toStringISO();

        /**
   * a boost representation of an unreferenced temporal point
   */
        boost::posix_time::ptime _pt;

        /**
  * Guessed resolution, i.e. least significant temporal element that is
  *specified in the given string
  **/
        TResolution _resolution;

      protected:
        /** year value */
        int _year;
        /** month value */
        int _month;
        /** week value */
        int _week;
        /** day of year value */
        int _doy;
        /** day of week value */
        int _dow;
        /** day of month value */
        int _dom;
        /** hour value */
        int _hour;
        /** minute value */
        int _minute;
        /** second value */
        int _second;
        /** fraction of second value */
        int _fraction;

        /** time zone hour value */
        int _tz_hour;
        /** time zone minute value */
        int _tz_minute;

      private:
        /**
   * @brief initiate the temporal point values
   *
   * @return void
   */
        void init();
    };

    /**
 * @brief A representation form of a temporal interval
 *
 * The TInterval is a class that is used to represent a temporal interval in one
 *of the following ways:
 * - number months
 * - number of years
 * - number of days
 * - exact time interval in seconds
 */
    class TInterval
    {
        friend class TReference;

        friend TInterval operator+(const TInterval& l, const TInterval& r);
        friend TInterval operator-(const TInterval& l, const TInterval& r);
        friend TInterval operator*(const TInterval& l, const int& r);
        friend TInterval operator*(const int& l, const TInterval& r);

      public:
        /**
   * @brief Basic constructor
   * Default constructor creates a time interval that equals 0.
   */
        TInterval();

        /**
  * @brief constructor with a ISO 8601 string.
  *
  * Construction from ISO 8601 string.
  *
  * @param str ISO 8601 string
  */
        TInterval(string str);

        /**
  * Duration given as number of months. Months vary in its number of days.
  */
        int _md;

        /**
  * Duration given as number of years. Years may or may not count the same
  * number of days, but always
  * have 12 months.
  */
        int _yd;

        /**
  * Duration given as number of days. Days may or may not count the same number
  * of seconds.
  */
        boost::gregorian::date_duration _dd;

        /**
  * Exact posix time difference (seconds).
  */
        boost::posix_time::time_duration _td;

        /**
  * Guessed resolution, most significant user-specified entry of a given string
  */
        TResolution _resolution;

        /**
   * @brief ISO 8601 string output
   *
   * @return std::string
   */
        std::string toStringISO();

      private:
        void init();
    };

    /**
 * Class that stores the temporal reference of an array, i.e. a start point and
*the cell size.
 * Methods to derive the (integer) dimension value for a given date / time and
*to get the date / time at a specific index are provided.
**/
    class TReference
    {
      public:
        /**
    * @brief Representation of a Temporal Reference
    *
    * Construction of a temporal reference based on a start point and a duration
    * representing the temporal distance between two neighbouring array cells.
    *Based on
    * the given duration, the temporal resolution is to some degree "guessed":
    * Examples:
    * - 1 day always represents a whole day, whether it has 23,24, or 25 hours;
    * - 1 month always represents a whole month and does not convert to days.
    * Combining incompatible date/time values in the cellsize might lead to
    *unpredictable results:
    * (Bad) Examples: P1MT1H (1 month + 1 hour)
    * (Good) Examples: P1M, P1Y6M, P16D, PT1H, PT30M, PT100S
    **/
        TReference(string t0text, string dttext);
        TReference();
        ~TReference();

        /**
   * @brief Getter for the temporal starting point
   *
   * Returns the start date/time, i.e. the date/time at dimension index 0
   *
   * @return scidb4geo::TPoint The temporal point at t0
   */
        TPoint getStart();

        /**
   * @brief Getter for the temporal resolution
   *
   * Returns the temporal distance between neighbouring cells
   *
   * @return scidb4geo::TInterval
   */
        TInterval getCellsize();

        /**
   * @brief Returns the date/time at a given index
   *
   * Calculates the date/time of an given index on the temporal dimension.
   *
   * @param index The index on the temporal dimension
   * @return scidb4geo::TPoint The calculated temporal point for an given index.
   */
        TPoint datetimeAtIndex(int index);

        /**
   * @brief Returns the index at a specific date/time.
   *
   * Calculates the temporal index of an array given a temporal point (TPoint).
   *The cells are assumed as
   * temporal periods (i.e. with a start and end time/date) within which values
   *do not change.
   *
   * @param t The TPoint representation of a certain date/time
   * @return int The nearest index for the given date/time
   */
        int indexAtDatetime(TPoint& t);

      protected:
        /** the temporal datum */
        TPoint* _t0;
        /** the temporal resolution (technically)*/
        TInterval* _dt;
        /** the  temporal resolution (informal) */
        TResolution _r;
    };
}

#endif
