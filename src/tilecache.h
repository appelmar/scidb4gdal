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


#ifndef TILECACHE_H
#define TILECACHE_H

#include <iostream>
#include <sstream>
#include <list>
#include <map>
#include "utils.h"

#define SCIDB4GEO_MAXCHUNKCACHE_MB 256

namespace scidb4gdal
{
    using namespace std;

    struct ArrayTile {
        ArrayTile() : data ( 0 ), size ( 0 ), id ( 0 ) {}
        void *data;
        size_t size;
        uint32_t id; // unique chunk id
    };

    /**
     * This class caches tiles read from SciDB by gdal locally in order to make writing SciDB arrays to line- or stripe-oriented image formats more efficient.
     */
    class TileCache
    {

    public:

        /**
         * Default constructor
         */
        TileCache();

        /**
         * Default  desctuctor gracefully releases memory
         */
        ~TileCache();


        /**
         * Checks whether a tile with given id is already cached
         * @param id unique tile id
         * @return true if tile is in cached
         */
        bool has ( uint32_t id );


        /**
         * Removes a tile with given id from cache
         * @param id unique tile id
         */
        void remove ( uint32_t id );

        /**
         * Adds a given tile to the cache
         * @param c the tile to be cached including its data pointer, size in bytes, and unique id
         */
        void add ( ArrayTile c );


        /**
         * Fetches a tile with given id from the cache
         * @param id unique tile id
         * @return Pointer to the requested tile including its data pointer, size in bytes, and unique id or null pointer if tile is not in cache
         */
        ArrayTile *get ( uint32_t id );

        /**
         * Clears the cache, i.e. removes all elements and frees memory
         */
        void clear();



        /**
         * Function for computing unique tile ids for two-dimensional multiband images
         * @param bx specific tile index in x direction
         * @param by specific tile index in y direction
         * @param by specific band index
         * @param nx total number of tiles in x direction
         * @param ny total number of tiles in y direction
         * @param nband total number of bands
         * @return unique id
         */
        static inline uint32_t getBlockId ( int bx, int by, int band, int nx, int ny, int nband ) {
            return ( band * nx * ny ) + ( by * nx ) + ( bx );
        }

        /**
        * Computes the available memory in bytes
         */
        inline size_t freeSpace() {
            return _maxSize - _totalSize;
        }



    private:

        size_t _totalSize;
        size_t _maxSize;
        map<uint32_t, ArrayTile> _cache; // TODO: unordered_map would be more efficient but C++11
        list<uint32_t> _q; // order of insertions for removing oldes first

    };


};



#endif
