#!/bin/bash
outputFolder="/tmp/"
cd "${outputFolder}"

#download chicago black-white geotiff image
chicago="./UTM2GTIF.TIF"
if (! test -f ${chicago})
then
#rm -f $chicago
wget "http://download.osgeo.org/geotiff/samples/spot/chicago/UTM2GTIF.TIF"
fi

#change connection details for scidb and uncomment it
#user="scidb"
#passwd="scidb"
#host="https://localhost"
#port=31000



#scidb array names
targetArray=test_chicago_s
targetArrayST=test_chicago_st
targetArraySTS=test_chicago_sts
targetArrayMetaNA=test_chicago_s_meta_na
targetArraySCov=test_chicago_s_cov
targetArraySTSCov=test_chicago_sts_cov

#output file names
rm -f ./test_*.tif
rm -f ./fail*.tif
outputS1="./test_s_con.tif"
outputS2="./test_s_oo.tif"
outputS3="./test_s_part_out.tif"
outputST1="./test_st.tif"
outputSTS1="./test_sts_index.tif"
outputSTS2="./test_sts_timestamp.tif"
outputSMetaNA="./test_s_meta_na.tif"
outputSCov="./test_s_cov.tif"

#create a log file
log="./test.log"
rm -f $log
touch $log
exec >> $log
exec 2>&1
exec 3>&1
exec 4>&1

echo ""
echo "#################################"
echo "# Preparing test data"
echo "#################################"
echo ""
#input image files prepare data for spatial insertion
chicago_part1="./chicago_1.tif"
chicago_part2="./chicago_2.tif"
chicago_part3="./chicago_3.tif"
chicago_part4="./chicago_4.tif"
chicago_na="./chicago_na.tif"

# echo rm -f $chicago_part1 $chicago_part2 $chicago_part3 $chicago_part4 $chicago_na
# rm -f $chicago_part1 $chicago_part2 $chicago_part3 $chicago_part4 $chicago_na
# echo ""

if (! test -f $chicago_part1)
then
  echo gdal_translate -of GTiff -srcwin 0 0 349 464 $chicago $chicago_part1
  gdal_translate -of GTiff -srcwin 0 0 349 464 $chicago $chicago_part1
  echo ""
else
  echo "$chicago_part1 does already exist. Skipping creation."
  echo ""
fi

if (! test -f $chicago_part2)
then
  echo gdal_translate -of GTiff -srcwin 350 0 349 464 $chicago $chicago_part2
  gdal_translate -of GTiff -srcwin 350 0 349 464 $chicago $chicago_part2
  echo ""
else
  echo "$chicago_part2 does already exist. Skipping creation."
  echo ""
fi

if (! test -f $chicago_part3)
then
  echo gdal_translate -of GTiff -srcwin 0 465 349 464 $chicago $chicago_part3
  gdal_translate -of GTiff -srcwin 0 465 349 464 $chicago $chicago_part3
  echo ""
else
  echo "$chicago_part3 does already exist. Skipping creation."
  echo ""
fi

if (! test -f $chicago_part4)
then
  echo gdal_translate -of GTiff -srcwin 350 465 349 464 $chicago $chicago_part4
  gdal_translate -of GTiff -srcwin 350 465 349 464 $chicago $chicago_part4
  echo ""
else
  echo "$chicago_part4 does already exist. Skipping creation."
  echo ""
fi

if (! test -f $chicago_na)
then
  echo gdal_translate -of GTiff -mo "TIFFTAG_IMAGEDESCRIPTION=Panchromatic image of chicago." -srcwin -50 -50 749 979 -a_nodata 0 $chicago $chicago_na
  gdal_translate -of GTiff -mo "TIFFTAG_IMAGEDESCRIPTION=Panchromatic image of chicago." -srcwin -50 -50 749 979 -a_nodata 0 $chicago $chicago_na
  echo ""
else
  echo "$chicago_na does already exist. Skipping creation."
  echo ""
fi

echo ""
echo "#################################"
echo "# Performing gdalinfo on test img"
echo "#################################"
echo ""
# echo gdalinfo $chicago_na
# gdalinfo $chicago_na
# echo ""

echo ""
echo "###########################################" 
echo "# Delete test Arrays if exists "
echo "###########################################"
echo ""

echo gdalmanage delete \"SCIDB:array=${targetArray} host=${host} port=${port} user=${user} password=${passwd}\"
gdalmanage delete "SCIDB:array=${targetArray} host=${host} port=${port} user=${user} password=${passwd}"
echo ""
echo gdalmanage delete \"SCIDB:array=${targetArrayST} host=${host} port=${port} user=${user} password=${passwd}\"
gdalmanage delete "SCIDB:array=${targetArrayST} host=${host} port=${port} user=${user} password=${passwd}"
echo ""
echo gdalmanage delete \"SCIDB:array=${targetArraySTS} host=${host} port=${port} user=${user} password=${passwd}\"
gdalmanage delete "SCIDB:array=${targetArraySTS} host=${host} port=${port} user=${user} password=${passwd}"
echo ""
echo gdalmanage delete \"SCIDB:array=${targetArrayMetaNA} host=${host} port=${port} user=${user} password=${passwd}\"
gdalmanage delete "SCIDB:array=${targetArrayMetaNA} host=${host} port=${port} user=${user} password=${passwd}"
echo ""
echo gdalmanage delete \"SCIDB:array=${targetArraySCov} host=${host} port=${port} user=${user} password=${passwd}\"
gdalmanage delete "SCIDB:array=${targetArraySCov} host=${host} port=${port} user=${user} password=${passwd}"
echo ""
echo gdalmanage delete \"SCIDB:array=${targetArraySTSCov} host=${host} port=${port} user=${user} password=${passwd}\"
gdalmanage delete "SCIDB:array=${targetArraySTSCov} host=${host} port=${port} user=${user} password=${passwd}"
echo ""

echo "" 
echo "###########################################"
echo "# Upload Spatial Images "
echo "###########################################"
echo ""

# #upload 1
echo "****** translate an image into a purely spatial array in scidb using create options"
echo gdal_translate --debug ON -co \"host=${host}\" -co \"port=${port}\" -co \"user=${user}\" -co \"password=${passwd}\" -co \"type=S\" -of SciDB ${chicago} \"SCIDB:array=${targetArray}\"
gdal_translate --debug ON -co "host=${host}" -co "port=${port}" -co "user=${user}" -co "password=${passwd}" -co "type=S" -of SciDB "${chicago}" "SCIDB:array=${targetArray}"
echo ""

#upload 2
echo "****** translate an image into a purely spatial array in scidb using create options, test for storing meta data and na_value"
echo gdal_translate --debug ON -co \"host=${host}\" -co \"port=${port}\" -co \"user=${user}\" -co \"password=${passwd}\" -co \"type=S\" -of SciDB ${chicago_na} \"SCIDB:array=${targetArrayMetaNA}\"
gdal_translate --debug ON -co "host=${host}" -co "port=${port}" -co "user=${user}" -co "password=${passwd}" -co "type=S" -of SciDB "${chicago_na}" "SCIDB:array=${targetArrayMetaNA}"
echo ""

echo "" 
echo "###########################################"
echo "# Upload Spatial Coverage "
echo "###########################################"
echo ""

echo "****** start a spatial coverage by stating a bounding box in the create options (bbox=left upper right lower)"
echo gdal_translate --debug ON -co \"host=${host}\" -co \"port=${port}\" -co \"user=${user}\" -co \"password=${passwd}\" -co \"bbox=443000 4650000 455000 4629000\" -co \"srs=${srs}\"  -co \"type=S\" -of SciDB ${chicago_part4} \"SCIDB:array=${targetArraySCov}\"
gdal_translate --debug ON -co "host=${host}" -co "port=${port}" -co "user=${user}" -co "password=${passwd}" -co "bbox=443000 4650000 455000 4629000" -co "srs=${srs}" -co "type=S" -of SciDB "${chicago_part4}" "SCIDB:array=${targetArraySCov}"
echo ""

echo "****** adding two more images simply by trying to translate to an existing array"
echo gdal_translate --debug ON -co \"host=${host}\" -co \"port=${port}\" -co \"user=${user}\" -co \"password=${passwd}\" -co \"type=S\" -of SciDB ${chicago_part1} \"SCIDB:array=${targetArraySCov}\"
gdal_translate --debug ON -co "host=${host}" -co "port=${port}" -co "user=${user}" -co "password=${passwd}" -co "type=S" -of SciDB "${chicago_part1}" "SCIDB:array=${targetArraySCov}"
echo ""
echo gdal_translate --debug ON -co \"host=${host}\" -co \"port=${port}\" -co \"user=${user}\" -co \"password=${passwd}\" -co \"type=S\" -of SciDB ${chicago_part3} \"SCIDB:array=${targetArraySCov}\"
gdal_translate --debug ON -co "host=${host}" -co "port=${port}" -co "user=${user}" -co "password=${passwd}" -co "type=S" -of SciDB "${chicago_part3}" "SCIDB:array=${targetArraySCov}"
echo ""
echo gdal_translate --debug ON -co \"host=${host}\" -co \"port=${port}\" -co \"user=${user}\" -co \"password=${passwd}\" -co \"type=S\" -of SciDB ${chicago_part2} \"SCIDB:array=${targetArraySCov}\"
gdal_translate --debug ON -co "host=${host}" -co "port=${port}" -co "user=${user}" -co "password=${passwd}" -co "type=S" -of SciDB "${chicago_part2}" "SCIDB:array=${targetArraySCov}"
echo ""

echo "" 
echo "##########################################################"
echo "# Upload Spatio Temporal single Image with create options "
echo "##########################################################"
echo "" 

#upload 3
echo "****** translate an image into spatially and temporally annotated SciDB array"
echo gdal_translate --debug ON -co \"host=${host}\" -co \"port=${port}\" -co \"user=${user}\" -co \"password=${passwd}\" -co \"dt=P1D\" -co \"t=2015-10-15T10:00:00\" -co \"type=ST\" -of SciDB ${chicago} \"SCIDB:array=${targetArrayST}\"
gdal_translate --debug ON -co "host=${host}" -co "port=${port}" -co "user=${user}" -co "password=${passwd}" -co "dt=P1D" -co "t=2015-10-15T10:00:00" -co "type=ST" -of SciDB "${chicago}" "SCIDB:array=${targetArrayST}"
echo "" 

echo "" 
echo "##########################################################"
echo "# Creating spatial and spatio-temporal image series"
echo "##########################################################"
echo "" 

#upload 4
echo "***** Start a spatial-temporal series"
echo gdal_translate --debug ON -co \"host=${host}\" -co \"port=${port}\" -co \"user=${user}\" -co \"password=${passwd}\" -co \"dt=P1D\" -co \"t=2015-10-15\" -co \"type=STS\" -of SciDB ${chicago} \"SCIDB:array=${targetArraySTS}\"
gdal_translate --debug ON -co "host=${host}" -co "port=${port}" -co "user=${user}" -co "password=${passwd}" -co "dt=P1D" -co "t=2015-10-15" -co "type=STS" -of SciDB "${chicago}" "SCIDB:array=${targetArraySTS}"
echo ""

#upload 5
echo "***** Insert second time slices"
echo gdal_translate --debug ON -co \"host=${host}\" -co \"port=${port}\" -co \"user=${user}\" -co \"password=${passwd}\" -co \"dt=P1D\" -co \"t=2015-10-20\" -co \"type=ST\" -of SciDB ${chicago} \"SCIDB:array=${targetArraySTS}\"
gdal_translate --debug ON -co "host=${host}" -co "port=${port}" -co "user=${user}" -co "password=${passwd}" -co "dt=P1D" -co "t=2015-10-20" -co "type=ST" -of SciDB "${chicago}" "SCIDB:array=${targetArraySTS}"
echo ""

#upload 6
echo "***** Insert third time slices inbetween"
echo gdal_translate --debug ON -co \"host=${host}\" -co \"port=${port}\" -co \"user=${user}\" -co \"password=${passwd}\" -co \"dt=P1D\" -co \"t=2015-10-17\" -co \"type=ST\" -of SciDB ${chicago} \"SCIDB:array=${targetArraySTS}\"
gdal_translate --debug ON -co "host=${host}" -co "port=${port}" -co "user=${user}" -co "password=${passwd}" -co "dt=P1D" -co "t=2015-10-17" -co "type=ST" -of SciDB "${chicago}" "SCIDB:array=${targetArraySTS}"
echo ""

#test the time series creation with a different temporal interval

echo "" 
echo "##########################################################"
echo "# Creating STS Coverages"
echo "##########################################################"
echo "" 

echo "***** Start a spatial-temporal series coverage"
echo gdal_translate --debug ON -co \"host=${host}\" -co \"port=${port}\" -co \"user=${user}\" -co \"password=${passwd}\" -co \"dt=P1D\" -co \"t=2015-10-15\" -co \"type=STS\" -co \"bbox=443000 4650000 455000 4629000\" -co \"srs=${srs}\" -of SciDB ${chicago_part2} \"SCIDB:array=${targetArraySTSCov}\"
gdal_translate --debug ON -co "host=${host}" -co "port=${port}" -co "user=${user}" -co "password=${passwd}" -co "dt=P1D" -co "t=2015-10-15" -co "type=STS" -co "bbox=443000 4650000 455000 4629000" -co "srs=${srs}" -of SciDB "${chicago_part2}" "SCIDB:array=${targetArraySTSCov}"
echo ""

echo "***** Add ST image to the first time slice"
echo gdal_translate --debug ON -co \"host=${host}\" -co \"port=${port}\" -co \"user=${user}\" -co \"password=${passwd}\" -co \"dt=P1D\" -co \"t=2015-10-15\" -co \"type=ST\" -of SciDB ${chicago_part1} \"SCIDB:array=${targetArraySTSCov}\"
gdal_translate --debug ON -co "host=${host}" -co "port=${port}" -co "user=${user}" -co "password=${passwd}" -co "dt=P1D" -co "t=2015-10-15" -co "type=ST" -of SciDB "${chicago_part1}" "SCIDB:array=${targetArraySTSCov}"
echo ""

echo "***** Add ST image to the second time slice"
echo gdal_translate --debug ON -co \"host=${host}\" -co \"port=${port}\" -co \"user=${user}\" -co \"password=${passwd}\" -co \"dt=P1D\" -co \"t=2015-10-16\" -co \"type=ST\" -of SciDB ${chicago_part3} \"SCIDB:array=${targetArraySTSCov}\"
gdal_translate --debug ON -co "host=${host}" -co "port=${port}" -co "user=${user}" -co "password=${passwd}" -co "dt=P1D" -co "t=2015-10-16" -co "type=ST" -of SciDB "${chicago_part3}" "SCIDB:array=${targetArraySTSCov}"
echo ""

echo "***** Add another ST image to the second time slice"
echo gdal_translate --debug ON -co \"host=${host}\" -co \"port=${port}\" -co \"user=${user}\" -co \"password=${passwd}\" -co \"dt=P1D\" -co \"t=2015-10-16\" -co \"type=ST\" -of SciDB ${chicago_part4} \"SCIDB:array=${targetArraySTSCov}\"
gdal_translate --debug ON -co "host=${host}" -co "port=${port}" -co "user=${user}" -co "password=${passwd}" -co "dt=P1D" -co "t=2015-10-16" -co "type=ST" -of SciDB "${chicago_part4}" "SCIDB:array=${targetArraySTSCov}"
echo ""

echo "" 
echo "##########################################################"
echo "# Info on different SciDB arrays"
echo "##########################################################"
echo "" 

echo "***** Info on simple spatial image using the connection file string method"
echo gdalinfo "SCIDB:host=${host} port=${port} user=${user} password=${passwd} array=${targetArray}"
gdalinfo "SCIDB:host=${host} port=${port} user=${user} password=${passwd} array=${targetArray}"
echo ""

echo "***** Info with connection options on the array with additional metadata and NA values"
echo gdalinfo --debug ON -oo "host=${host}" -oo "port=${port}" -oo "user=${user}" -oo "password=${passwd}" "SCIDB:array=${targetArrayMetaNA}"
gdalinfo --debug ON -oo "host=${host}" -oo "port=${port}" -oo "user=${user}" -oo "password=${passwd}" "SCIDB:array=${targetArrayMetaNA}"
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

echo "***** Downloading subset of spatial image using connection string"
echo gdal_translate --debug ON -srcwin 0 0 200 200 -of GTiff \"SCIDB:host=${host} port=${port} user=${user} password=${passwd} array=${targetArray}\" ${outputS1} 
gdal_translate --debug ON -srcwin 0 0 200 200 -of GTiff "SCIDB:host=${host} port=${port} user=${user} password=${passwd} array=${targetArray}" ${outputS1} 
echo ""


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

echo "***** Downloading subset of spatio-temporal image using opening options with temporal index from STS"
echo gdal_translate --debug ON -srcwin 0 0 200 200 -oo \"host=${host}\" -oo \"port=${port}\" -oo \"user=${user}\" -oo \"password=${passwd}\" -of GTiff \"SCIDB:array=${targetArraySTS}[i,5]\" ${outputSTS2}
gdal_translate --debug ON -srcwin 0 0 200 200 -oo "host=${host}" -oo "port=${port}" -oo "user=${user}" -oo "password=${passwd}" -of GTiff "SCIDB:array=${targetArraySTS}[i,5]" ${outputSTS1}
echo ""

echo "***** Downloading subset of spatio-temporal image using opening options with temporal index (just one image in)"
echo gdal_translate --debug ON -srcwin 0 0 200 200 -oo \"host=${host}\" -oo \"port=${port}\" -oo \"user=${user}\" -oo \"password=${passwd}\" -of GTiff \"SCIDB:array=${targetArraySTS}[t,2015-10-17]\" ${outputSTS2}
gdal_translate --debug ON -srcwin 0 0 200 200 -oo "host=${host}" -oo "port=${port}" -oo "user=${user}" -oo "password=${passwd}" -of GTiff "SCIDB:array=${targetArraySTS}[t,2015-10-17]" ${outputSTS2}
echo ""

echo "***** Downloading image with additional meta data and NA value"
echo gdal_translate --debug ON -oo \"host=${host}\" -oo \"port=${port}\" -oo \"user=${user}\" -oo \"password=${passwd}\" -of GTiff \"SCIDB:array=${targetArrayMetaNA}\" ${outputSMetaNA}
gdal_translate --debug ON -oo "host=${host}" -oo "port=${port}" -oo "user=${user}" -oo "password=${passwd}" -of GTiff "SCIDB:array=${targetArrayMetaNA}" ${outputSMetaNA}
echo ""

echo "***** Downloading the coverage"
echo gdal_translate --debug ON -oo \"host=${host}\" -oo \"port=${port}\" -oo \"user=${user}\" -oo \"password=${passwd}\" -of GTiff \"SCIDB:array=${targetArraySCov}\" ${outputSCov}
gdal_translate --debug ON -oo "host=${host}" -oo "port=${port}" -oo "user=${user}" -oo "password=${passwd}" -of GTiff "SCIDB:array=${targetArraySCov}" ${outputSCov}
echo ""

echo "***** Downloading the coverages of the ST coverage (index 0 and 1)"
echo gdal_translate --debug ON -oo \"host=${host}\" -oo \"port=${port}\" -oo \"user=${user}\" -oo \"password=${passwd}\" -of GTiff \"SCIDB:array=${targetArraySTSCov}[i,0]\" ${outputSTSCov1}
gdal_translate --debug ON -oo "host=${host}" -oo "port=${port}" -oo "user=${user}" -oo "password=${passwd}" -of GTiff "SCIDB:array=${targetArraySTSCov}[i,0]" ${outputSTSCov1}
echo ""
echo gdal_translate --debug ON -oo \"host=${host}\" -oo \"port=${port}\" -oo \"user=${user}\" -oo \"password=${passwd}\" -of GTiff \"SCIDB:array=${targetArraySTSCov}[i,1]\" ${outputSTSCov2}
gdal_translate --debug ON -oo "host=${host}" -oo "port=${port}" -oo "user=${user}" -oo "password=${passwd}" -of GTiff "SCIDB:array=${targetArraySTSCov}[i,1]" ${outputSTSCov2}
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

echo "***** create coverage with bbox and w/o SRS statement"
echo gdal_translate --debug ON -co \"host=${host}\" -co \"port=${port}\" -co \"user=${user}\" -co \"password=${passwd}\" -co \"bbox=443000 4650000 455000 4629000\" -co \"srs=${srs}\"  -co \"type=S\" -of SciDB ${chicago_part4} \"SCIDB:array=fail_bbox_srs\"
gdal_translate --debug ON -co "host=${host}" -co "port=${port}" -co "user=${user}" -co "password=${passwd}" -co "bbox=443000 4650000 455000 4629000" -co "srs=${srs}" -co "type=S" -of SciDB "${chicago_part4}" "SCIDB:array=fail_bbox_srs"
echo ""