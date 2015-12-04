#!/bin/bash
outputFolder="/tmp/"
cd "${outputFolder}"

#download chicago black-white geotiff image
rm -f "./UTM2GTIF.TIF"
wget "http://download.osgeo.org/geotiff/samples/spot/chicago/UTM2GTIF.TIF"

#change connection details for scidb and uncomment it
#user="scidb"
#passwd="scidb"
#host="https://localhost"
#port=31000

#input image files
chicago="./UTM2GTIF.TIF"

#scidb array names
targetArray=test_chicago_s
targetArrayST=test_chicago_st
targetArraySTS=test_chicago_sts

#output file names
rm -f ./test_*.tif
rm -f ./fail*.tif
outputS1="./test_s_con.tif"
outputS2="./test_s_oo.tif"
outputS3="./test_s_part_out.tif"
outputST1="./test_st.tif"
outputSTS1="./test_sts_index.tif"
outputSTS2="./test_sts_timestamp.tif"

#create a log file
log="./test.log"
rm -f $log
touch $log
exec >> $log
exec 2>&1
exec 3>&1
exec 4>&1

echo "#################################"
echo "# Performing gdalinfo on test img"
echo "#################################"
gdalinfo $chicago
echo ""

echo "###########################################" 
echo "# Delete test Arrays if exists "
echo "###########################################"

echo gdalmanage delete "SCIDB:array=${targetArray} host=${host} port=${port} user=${user} password=${passwd}"
gdalmanage delete "SCIDB:array=${targetArray} host=${host} port=${port} user=${user} password=${passwd}"
echo gdalmanage delete "SCIDB:array=${targetArrayST} host=${host} port=${port} user=${user} password=${passwd}"
gdalmanage delete "SCIDB:array=${targetArrayST} host=${host} port=${port} user=${user} password=${passwd}"
echo gdalmanage delete "SCIDB:array=${targetArraySTS} host=${host} port=${port} user=${user} password=${passwd}"
gdalmanage delete "SCIDB:array=${targetArraySTS} host=${host} port=${port} user=${user} password=${passwd}"

echo "" 
echo "###########################################"
echo "# Upload Spatial Image with create options "
echo "###########################################"
echo ""

#upload 1
echo "****** translate an image into a purely spatial array in scidb using create options"
echo gdal_translate --debug ON -co \"host=${host}\" -co \"port=${port}\" -co \"user=${user}\" -co \"password=${passwd}\" -co \"type=S\" -of SciDB ${chicago} \"SCIDB:array=${targetArray}\"
gdal_translate --debug ON -co "host=${host}" -co "port=${port}" -co "user=${user}" -co "password=${passwd}" -co "type=S" -of SciDB "${chicago}" "SCIDB:array=${targetArray}"
echo ""

echo "" 
echo "##########################################################"
echo "# Upload Spatio Temporal single Image with create options "
echo "##########################################################"
echo "" 

#upload 2
echo "****** translate an image into spatially and temporally annotated SciDB array"
echo gdal_translate --debug ON -co \"host=${host}\" -co \"port=${port}\" -co \"user=${user}\" -co \"password=${passwd}\" -co \"dt=P1D\" -co \"t=2015-10-15T10:00:00\" -co \"type=ST\" -of SciDB ${chicago} \"SCIDB:array=${targetArrayST}\"
gdal_translate --debug ON -co "host=${host}" -co "port=${port}" -co "user=${user}" -co "password=${passwd}" -co "dt=P1D" -co "t=2015-10-15T10:00:00" -co "type=ST" -of SciDB "${chicago}" "SCIDB:array=${targetArrayST}"
echo "" 

echo "" 
echo "##########################################################"
echo "# Creating spatial and spatio-temporal image series"
echo "##########################################################"
echo "" 

#upload 3
echo "***** Start a spatial-temporal series"
echo gdal_translate --debug ON -co \"host=${host}\" -co \"port=${port}\" -co \"user=${user}\" -co \"password=${passwd}\" -co \"dt=P1D\" -co \"t=2015-10-15\" -co \"type=STS\" -of SciDB ${chicago} \"SCIDB:array=${targetArraySTS}\"
gdal_translate --debug ON -co "host=${host}" -co "port=${port}" -co "user=${user}" -co "password=${passwd}" -co "dt=P1D" -co "t=2015-10-15" -co "type=STS" -of SciDB "${chicago}" "SCIDB:array=${targetArraySTS}"
echo ""

#upload 4
echo "***** Insert second time slices"
echo gdal_translate --debug ON -co \"host=${host}\" -co \"port=${port}\" -co \"user=${user}\" -co \"password=${passwd}\" -co \"dt=P1D\" -co \"t=2015-10-20\" -co \"type=ST\" -of SciDB ${chicago} \"SCIDB:array=${targetArraySTS}\"
gdal_translate --debug ON -co "host=${host}" -co "port=${port}" -co "user=${user}" -co "password=${passwd}" -co "dt=P1D" -co "t=2015-10-20" -co "type=ST" -of SciDB "${chicago}" "SCIDB:array=${targetArraySTS}"
echo ""

#upload 5
echo "***** Insert third time slices inbetween"
echo gdal_translate --debug ON -co \"host=${host}\" -co \"port=${port}\" -co \"user=${user}\" -co \"password=${passwd}\" -co \"dt=P1D\" -co \"t=2015-10-17\" -co \"type=ST\" -of SciDB ${chicago} \"SCIDB:array=${targetArraySTS}\"
gdal_translate --debug ON -co "host=${host}" -co "port=${port}" -co "user=${user}" -co "password=${passwd}" -co "dt=P1D" -co "t=2015-10-17" -co "type=ST" -of SciDB "${chicago}" "SCIDB:array=${targetArraySTS}"
echo ""

#test the time series creation with a different temporal interval

echo "" 
echo "##########################################################"
echo "# Info on different SciDB arrays"
echo "##########################################################"
echo "" 

echo "***** Info on simple spatial image using the connection file string method"
echo gdalinfo "SCIDB:host=${host} port=${port} user=${user} password=${passwd} array=${targetArray}"
gdalinfo "SCIDB:host=${host} port=${port} user=${user} password=${passwd} array=${targetArray}"
echo ""

echo "***** Info on time slice in a spatio-temporal image series"
echo gdalinfo --debug ON -oo "host=${host}" -oo "port=${port}" -oo "user=${user}" -oo "password=${passwd}" "SCIDB:array=${targetArraySTS}[i,2]"
gdalinfo --debug ON -oo "host=${host}" -oo "port=${port}" -oo "user=${user}" -oo "password=${passwd}" "SCIDB:array=${targetArraySTS}[i,2]"
echo ""

echo "" 
echo "##########################################################"
echo "# Retrieving different kinds of images from SciDB"
echo "##########################################################"
echo "" 
#to speed up the download part we restrict the image windows to 200x200 pixel
#outputS1="./test_s_con.tif"
echo "***** Downloading subset of spatial image using connection string"
echo gdal_translate --debug ON -srcwin 0 0 200 200 -of GTiff \"SCIDB:host=${host} port=${port} user=${user} password=${passwd} array=${targetArray}\" ${outputS1} 
gdal_translate --debug ON -srcwin 0 0 200 200 -of GTiff "SCIDB:host=${host} port=${port} user=${user} password=${passwd} array=${targetArray}" ${outputS1} 
echo ""

#outputS2="./test_s_oo.tif"
echo "***** Downloading subset of spatial image using opening options"
echo gdal_translate --debug ON -srcwin 0 0 200 200 -oo \"host=${host}\" -oo \"port=${port}\" -oo \"user=${user}\" -oo \"password=${passwd}\" -of GTiff \"SCIDB:array=${targetArray}\" ${outputS2}
gdal_translate --debug ON -srcwin 0 0 200 200 -oo "host=${host}" -oo "port=${port}" -oo "user=${user}" -oo "password=${passwd}" -of GTiff "SCIDB:array=${targetArray}" ${outputS2}
echo ""

echo "***** Downloading subset of spatial image using opening options that is partially out"
echo gdal_translate --debug ON -srcwin -50 -50 150 150 -oo \"host=${host}\" -oo \"port=${port}\" -oo \"user=${user}\" -oo \"password=${passwd}\" -of GTiff \"SCIDB:array=${targetArray}\" ${outputS3}
gdal_translate --debug ON -srcwin -50 -50 150 150 -oo "host=${host}" -oo "port=${port}" -oo "user=${user}" -oo "password=${passwd}" -of GTiff "SCIDB:array=${targetArray}" ${outputS3}
echo ""

echo "***** Downloading subset of spatio-temporal image using opening options with temporal index (just one image in)"
echo gdal_translate --debug ON -srcwin 0 0 200 200 -oo \"host=${host}\" -oo \"port=${port}\" -oo \"user=${user}\" -oo \"password=${passwd}\" -oo \"i=0\" -of GTiff \"SCIDB:array=${targetArrayST}\" ${outputST1}
gdal_translate --debug ON -srcwin 0 0 200 200 -oo "host=${host}" -oo "port=${port}" -oo "user=${user}" -oo "password=${passwd}" -oo "i=0" -of GTiff "SCIDB:array=${targetArrayST}" ${outputST1}
echo ""

#outputSTS1="./test_sts_index.tif"
echo "***** Downloading subset of spatio-temporal image using opening options with temporal index from STS"
echo gdal_translate --debug ON -srcwin 0 0 200 200 -oo \"host=${host}\" -oo \"port=${port}\" -oo \"user=${user}\" -oo \"password=${passwd}\" -of GTiff \"SCIDB:array=${targetArraySTS}[i,5]\" ${outputSTS2}
gdal_translate --debug ON -srcwin 0 0 200 200 -oo "host=${host}" -oo "port=${port}" -oo "user=${user}" -oo "password=${passwd}" -of GTiff "SCIDB:array=${targetArraySTS}[i,5]" ${outputSTS1}
echo ""

#outputSTS2="./test_sts_timestamp.tif"
echo "***** Downloading subset of spatio-temporal image using opening options with temporal index (just one image in)"
echo gdal_translate --debug ON -srcwin 0 0 200 200 -oo \"host=${host}\" -oo \"port=${port}\" -oo \"user=${user}\" -oo \"password=${passwd}\" -of GTiff \"SCIDB:array=${targetArraySTS}[t,2015-10-17]\" ${outputSTS2}
gdal_translate --debug ON -srcwin 0 0 200 200 -oo "host=${host}" -oo "port=${port}" -oo "user=${user}" -oo "password=${passwd}" -of GTiff "SCIDB:array=${targetArraySTS}[t,2015-10-17]" ${outputSTS2}
echo ""

echo "" 
echo "##########################################################"
echo "# Faulty queries and captured errors"
echo "##########################################################"
echo "" 

echo "***** interval brackets wrong"
outputFail1="fail1.tif"
echo gdal_translate --debug ON -oo \"host=${host}\" -oo \"port=${port}\" -oo \"user=${user}\" -oo \"password=${passwd}\" -of GTiff -b 1 -srcwin 0 0 200 200 \"SCIDB:array=${targetArraySTS}[i=1[\" ${outputFail1}
gdal_translate --debug ON -oo "host=${host}" -oo "port=${port}" -oo "user=${user}" -oo "password=${passwd}" -of GTiff -b 1 -srcwin 0 0 200 200 "SCIDB:array=${targetArraySTS}[i=1[" ${outputFail1}
echo "" 

echo "***** temp string malformed"
outputFail2="fail2.tif"
echo gdal_translate --debug ON -oo \"host=${host}\" -oo \"port=${port}\" -oo \"user=${user}\" -oo \"password=${passwd}\" -of GTiff -b 1 -srcwin 0 0 200 200 \"SCIDB:array=${targetArraySTS}[t=20151020]\" ${outputFail2}
gdal_translate --debug ON -oo "host=${host}" -oo "port=${port}" -oo "user=${user}" -oo "password=${passwd}" -of GTiff -b 1 -srcwin 0 0 200 200 "SCIDB:array=${targetArraySTS}[t=20151020]" ${outputFail2}
echo ""

echo "***** out of bounce T"
outputFail3="fail3.tif"
echo gdal_translate --debug ON -oo \"host=${host}\" -oo \"port=${port}\" -oo \"user=${user}\" -oo \"password=${passwd}\" -oo "i=100" -of GTiff -b 1 -srcwin 0 0 200 200 \"SCIDB:array=${targetArraySTS}\" ${outputFail3}
gdal_translate --debug ON -oo "host=${host}" -oo "port=${port}" -oo "user=${user}" -oo "password=${passwd}" -oo "i=100" -of GTiff -b 1 -srcwin 0 0 200 200 "SCIDB:array=${targetArraySTS}" ${outputFail3}
echo ""

echo "***** array does not exist"
outputFail4="fail4.tif"
echo gdal_translate --debug ON -oo \"host=${host}\" -oo \"port=${port}\" -oo \"user=${user}\" -oo \"password=${passwd}\" -oo \"i=100\" -of GTiff -b 1 -srcwin 0 0 200 200 \"SCIDB:array=someArrayWhatever\" ${outputFail4}
gdal_translate --debug ON -oo "host=${host}" -oo "port=${port}" -oo "user=${user}" -oo "password=${passwd}" -oo "i=100" -of GTiff -b 1 -srcwin 0 0 200 200 "SCIDB:array=someArrayWhatever" ${outputFail4}
echo ""

echo "***** info on temporal image without slice"
echo gdalinfo -oo \"host=${host}\" -oo \"port=${port}\" -oo \"user=${user}\" -oo \"password=${passwd}\" \"SCIDB:array=${targetArraySTS}\"
gdalinfo -oo "host=${host}" -oo "port=${port}" -oo "user=${user}" -oo "password=${passwd}" "SCIDB:array=${targetArraySTS}"
echo ""

