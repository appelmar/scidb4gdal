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

#include "scidb_structs.h"

namespace scidb4gdal {
    using namespace std;

    string SciDBArray::toString() {
        stringstream s;
        s << "'" << name << "'"
        << ":";
        for (uint32_t i = 0; i < dims.size(); ++i)
            s << "<'" << dims[i].name << "'," << dims[i].low << ":" << dims[i].high
            << "," << dims[i].typeId << ">";
        for (uint32_t i = 0; i < attrs.size(); ++i)
            s << "['" << attrs[i].name << "'," << attrs[i].typeId << ","
            << attrs[i].nullable << "]";
        s << "\n";
        return s.str();
    }

    string SciDBArray::getFormatString() {
        stringstream s;
        s << "(";
        for (uint32_t i = 0; i < attrs.size() - 1; ++i) {
            s << attrs[i].typeId << ","; // TODO: Add nullable
        }
        s << attrs[attrs.size() - 1].typeId; // TODO: Add nullable
        s << ")";
        return s.str();
    }

    string SciDBArray::getSchemaString() {
        stringstream s;

        s << "<";
        for (uint32_t i = 0; i < attrs.size(); ++i) {
            s << attrs[i].name << ":" << attrs[i].typeId;
            if (attrs[i].nullable)
                s << " null";
            if (i < (attrs.size() - 1))
                s << ",";
        }
        s << ">";

        s << "[";
        for (uint32_t i = 0; i < dims.size(); ++i) {
            s << dims[i].name << "=" << dims[i].start << ":"
            << (dims[i].start + dims[i].length - 1);
            s << "," << dims[i].chunksize << ","
            << "0"; // TODO: Add overlap
            if (i < (dims.size() - 1))
                s << ",";
        }
        s << "]";
        return s.str();
    }
}
