#
# Copyright (c) 2016 Marius Appel <marius.appel@uni-muenster.de>
#
# This file is part of scidb4gdal. scidb4gdal is licensed under the MIT license.
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
#


import os
import sys
import argparse
import glob
import logging
import subprocess
import datetime

# This script does nothing else than calling gdal_translate in a loop over a set of input files.
# Requires python version > 3.3 due to print(...,flush=True)
# Example:
# python3 srtm2scidb.py -d /home/xyz/srtm -a srtm_array -ymin 30.0 -xmin -12.0 -ymax 61.0 -xmax 31.0 -dbhost https://localhost -dbport 8083 -dbuser scidb -dbpassword scidb -srs EPSG:4326 -v


def main(argv):
    parser = argparse.ArgumentParser(description="Creates a SciDB array from SRTM files")
    parser.add_argument("-d", help="Directory containing SRTM GeoTIFF files", required=True)
    parser.add_argument("-a", help="Name of the SciDB target array", required=True)
    parser.add_argument("-xmin", help="Minimum x coordinate of the array", default=-180, type=float)
    parser.add_argument("-xmax", help="Maximum x coordinate of the array", default=180, type=float)
    parser.add_argument("-ymin", help="Minimum y coordinate of the array", default=-90, type=float)
    parser.add_argument("-ymax", help="Maximum y coordinate of the array", default=90, type=float)
    parser.add_argument("-srs", help="SRS identifier", default="EPSG:4326")
    parser.add_argument("-dbhost", help="SciDB hostname", default="")
    parser.add_argument("-dbport", help="SciDB port (Shim)", default="")
    parser.add_argument("-dbuser", help="Username to connect to SciDB", default="")
    parser.add_argument("-dbpassword", help="Password to connect to SciDB", default="")
    parser.add_argument("-gdaldir", help="Directory of GDAL binaries", default="")
    parser.add_argument("-v", help="Print verbose messages", action='store_true')

    args = parser.parse_args()

    start_time = datetime.datetime.now()
    print("SRTM2SCIDB STARTED AT " +  str(start_time), flush=True)

    gdalcmd = "gdal_translate"
    if len(args.gdaldir) > 0:
        gdalcmd = args.gdaldir + "/" + gdalcmd

    # Create array with first image
    files = glob.glob(args.d + "/" + "*.tif")
    if len(files) == 0:
        print("No matching files found.", flush=True)
        quit()

    ntiles = len(files)
    curtile = 1

    f0 = files.pop()
    try:
        if args.v:
            print("Trying to ingest first image: " + f0, flush=True)

        # Build gdal_translate command string depending on input args
        syscall = [gdalcmd]
        if len(args.dbhost) > 0:
            syscall.extend(["-co", "host=" + args.dbhost])
        if len(args.dbport) > 0:
            syscall.extend(["-co", "port=" + args.dbport])
        if len(args.dbuser) > 0:
            syscall.extend(["-co", "user=" + args.dbuser])
        if len(args.dbpassword) > 0:
            syscall.extend(["-co", "password=" + args.dbpassword])
        syscall.extend(["-co", "bbox=" + str(args.xmin) + " " + str(args.ymin) + " " + str(args.xmax) + " " + str(args.ymax) ])
        syscall.extend(["-co", "srs=" + args.srs])
        syscall.extend(["-co", "type=S"])
        syscall.extend(["-of", "SciDB", f0, "SCIDB:array=" + args.a])

        if args.v:
            print("Performing system command: " + ' '.join(syscall), flush=True)
        out = subprocess.check_output(syscall,stderr=subprocess.STDOUT)
        if args.v:
            print(out, flush=True)

    except subprocess.CalledProcessError as e:
        print("FAILED: subprocess returned code " + str(e.returncode) + ": " + "COMMAND: " + str(e.cmd) + " ARGS: " + str(e.args) + " OUTPUT: " + str(e.output), flush=True)
        quit()

    except:
        e = sys.exc_info()[0]
        print("FAILED: Unknown exception: " + str(e.message), flush=True)
        quit()

    for f in files:
        curtile += 1
        try:
            if args.v:
                print(str(datetime.datetime.now()) + ": (" + str(curtile) + "/" + str(ntiles) + ")", flush=True)
                print("Trying to ingest image: " + f, flush=True)

            syscall = [gdalcmd]
            if len(args.dbhost) > 0:
                syscall.extend(["-co", "host=" + args.dbhost])
            if len(args.dbport) > 0:
                syscall.extend(["-co", "port=" + args.dbport])
            if len(args.dbuser) > 0:
                syscall.extend(["-co", "user=" + args.dbuser])
            if len(args.dbpassword) > 0:
                syscall.extend(["-co", "password=" + args.dbpassword])
            syscall.extend(["-co", "type=S"])
            syscall.extend(["-of", "SciDB", f, "SCIDB:array=" + args.a])

            if args.v:
                print("Performing system command: " + ' '.join(syscall), flush=True)
            out = subprocess.check_output(syscall,stderr=subprocess.STDOUT)
            if args.v:
                print(out, flush=True)

        except subprocess.CalledProcessError as e:
            print("FAILED: subprocess returned code " + str(e.returncode) + ": " + "COMMAND: " + str(e.cmd) + " ARGS: " + str(e.args) + " OUTPUT: " + str(e.output), flush=True)

        except:
            e = sys.exc_info()[0]
            print("FAILED: Unknown exception: " + str(e.message), flush=True)

    end_time = datetime.datetime.now()
    print("SRTM2SCIDB FINISHED AT " + str(end_time), flush=True)

    duration = end_time - start_time
    print("TOTAL RUNTIME: " + str(duration), flush=True)
    print("DONE.", flush=True)



if __name__ == "__main__":
    main(sys.argv[1:])
