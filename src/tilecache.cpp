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

#include "tilecache.h"



namespace scidb4gdal
{

    TileCache::TileCache() : _totalSize ( 0 ), _maxSize ( SCIDB4GEO_MAXCHUNKCACHE_MB * 1024 * 1024 ) {}

    TileCache::~TileCache()
    {
        clear();
        _cache.clear();
        _q.clear();
    }

    bool TileCache::has ( uint32_t id )
    {
        map<uint32_t, ArrayTile>::iterator it;
        return _cache.find ( id ) != _cache.end();
    }


    void TileCache::remove ( uint32_t id )
    {
//  stringstream ss;
//  ss << "Removing tile " << id << " from local tile cache";
//  Utils::debug(ss.str());
        map<uint32_t, ArrayTile>::iterator it = _cache.find ( id );
        if ( it != _cache.end() ) {
            ArrayTile temp = it->second;
            free ( temp.data );
            _totalSize -= temp.size;
            _cache.erase ( it );
            _q.remove ( id );
        }
    }


    void TileCache::clear()
    {
        while ( !_q.empty() ) {
            remove ( _q.front() );
        }
    }


    void TileCache::add ( ArrayTile c )
    {
        // Assert that chunk has not been cached already
        if ( has ( c.id ) ) return;


//  stringstream ss;
//  ss << "Adding tile " << c.id << " to local tile cache";
//  Utils::debug(ss.str());

        // Check whether enough memory, if not, delete front (oldest) element
        while ( freeSpace() < c.size ) {
            if ( _q.empty() ) {
                Utils::warn ( "Local array tile cache to small to store a single chunk, please consider either increasing local cache size or reducing gdal block size" );
                return;
            }
            remove ( _q.front() );
        }
        _cache[c.id] = c;
        _q.push_back ( c.id ); //
        _totalSize += c.size;
    }


    ArrayTile *TileCache::get ( uint32_t id )
    {
//  stringstream ss;
//  ss << "Fetching tile " << id << " from local tile cache";
//  Utils::debug(ss.str());
        if ( has ( id ) )
            return &_cache[id];
        return NULL;
    }









}
