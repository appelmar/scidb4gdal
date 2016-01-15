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
    
    /**
     * @brief An array tile that is used to represent data of one chunk in SciDB
     * 
     * The Array Tile will hold the data of a chunk as well as the size of the held data and the id of the tile
     * if refers to.
     */
    struct ArrayTile {
	/** @brief Basic constructor */
        ArrayTile() : data ( 0 ), size ( 0 ), id ( 0 ) {}
        /** the data */
        void *data;
	/** the size of the data in memory */
        size_t size;
	/** the id of tile / chunk */ 
        uint32_t id;
    };

    /**
     * @brief A cache for various chunks of an SciDB array.
     * 
     * This class caches tiles that were read from SciDB by gdal locally. Because some formats that are line- or stripe oriented require to read a whole line
     * in order to run efficiently.
     */
    class TileCache
    {

    public:

        /**
         * @brief Default constructor
         */
        TileCache();

        /**
         * @brief Default destructor gracefully releases memory
         */
        ~TileCache();


        /**
         * @brief Checks whether a tile with given id is already cached
         * @param id unique tile id
         * @return true if tile is in cached
         */
        bool has ( uint32_t id );


        /**
	 * @brief Removes a tile with given id from cache
         * @param id unique tile id
         */
        void remove ( uint32_t id );

        /**
         * @brief add a tile to the cache
         * @param c the tile to be cached including its data pointer, size in bytes, and unique id
         */
        void add ( ArrayTile c );


        /**
         * @brief Fetches a tile with given id from the cache
         * @param id unique tile id
         * @return Pointer to the requested tile including its data pointer, size in bytes, and unique id or null pointer if tile is not in cache
         */
        ArrayTile *get ( uint32_t id );

        /**
         * @brief Clears the cache
	 * 
	 * Clears the cache by removing all tiles and freeing the memory
	 * 
	 * @return void
         */
        void clear();



        /**
         * @brief calculates unique tile id
	 * 
	 * Function for computing unique tile ids for two-dimensional multiband images
	 * 
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
	 * @brief Computes the available memory in bytes
	 * @return size_t remaining size
         */
        inline size_t freeSpace() {
            return _maxSize - _totalSize;
        }



    private:
	/** the total size of the cached image */
        size_t _totalSize;
	/** the maximum size that is reserved */
        size_t _maxSize;
	/** a look up table to relate unique ids and the array tile that is referred to */
        map<uint32_t, ArrayTile> _cache; // TODO: unordered_map would be more efficient but C++11
        /** order of insertions for removing oldes first */
        list<uint32_t> _q; 
    };
};



#endif
