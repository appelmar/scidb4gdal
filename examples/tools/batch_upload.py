#!/usr/bin/python

#
# parts were taken from the tutorial at https://pcjericks.github.io/py-gdalogr-cookbook/gdal_general.html
#

import sys, math, getopt, time
try:
    from osgeo import ogr, osr, gdal
    from subprocess import call
except:
    sys.exit('ERROR: cannot find GDAL/OGR modules')
    
import os
from os.path import isfile, join

def printHelp():
    print 'batch_upload.py -d <inputdir> -a <array_name>'
    print "Options:"
    print "\t\t--help=\t\t This help file"
    print ""
    print '\t-d\t\t\t The input directory'
    print "\t\t--dir="
    print ""
    print '\t-a\t\t\t The name of the target array'
    print "\t\t--array="
    print ""
    print '\t-h\t\t\t URL for the SHIM client'
    print "\t\t--host="
    print ""
    print '\t-p\t\t\t The SHIM client port on the host'
    print "\t\t--port="
    print ""
    print '\t-u\t\t\t A registered user at the SHIM client'
    print "\t\t--user="
    print ""
    print '\t-w\t\t\t The users password'
    print "\t\t--pwd="
    print ""
    print "\t\t--border=\t The size of the border in percent. Default 0.1% = 0.001"
    print ""
    print "\t\t--chunk_sp=\t The spatial chunksize for the target array"
    print ""
    print "\t\t--chunk_t=\t The temporal chunksize for the target array"
    print ""
    print "\t\t--t_srs=\t The target reference system in case the images have different spatial reference systems"
    sys.exit()

# http://gis.stackexchange.com/questions/57834/how-to-get-raster-corner-coordinates-using-python-gdal-bindings
def GetExtent(gt,cols,rows):
    ext=[]
    xarr=[0,cols]
    yarr=[0,rows]

    for px in xarr:
	for py in yarr:
	    x=gt[0]+(px*gt[1])+(py*gt[2])
	    y=gt[3]+(px*gt[4])+(py*gt[5])
	    ext.append([x,y])
	yarr.reverse()
    return ext
  
# example GDAL error handler function
def gdal_error_handler(err_class, err_num, err_msg):
    errtype = {
	    gdal.CE_None:'None',
	    gdal.CE_Debug:'Debug',
	    gdal.CE_Warning:'Warning',
	    gdal.CE_Failure:'Failure',
	    gdal.CE_Fatal:'Fatal'
    }
    err_msg = err_msg.replace('\n',' ')
    err_class = errtype.get(err_class, 'None')
    print '%s : %s' % (err_class,err_msg)

def warp(file,authority,srs):
    warp_append="_warped"
    path,ending=os.path.splitext(file)
    warped_file = path+warp_append+ending
    if (not os.path.isfile(warped_file)):
	warp_command = ["gdalwarp","-t_srs", authority+":"+srs, "-r", "bilinear", "-of", "GTiff", file, warped_file]
	print ("gdalwarp -t_srs "+authority+":"+srs+" -r bilinear -of GTiff "+file+" "+warped_file)
	try: 
	  call(warp_command)
	except:
	  sys.exit(0)
	return warped_file
    else:
	print warped_file+" already exists."
	return None
    

def parseParameter(argv):
  global directory
  global array
  global host
  global port
  global user
  global pwd
  global con
  global border
  global ch_sp
  global ch_t
  global t_srs
  array=None
  directory=None
  host=port=user=pwd=t_srs=None
  con=0
  border=ch_sp=ch_t=None
  if (len(argv) == 0):
    printHelp()
    sys.exit(2)
  
  try:
      opts, args = getopt.getopt(argv,"h:d:a:p:u:w:",["help=","dir=","array=","host=","port=","user=","pwd=","border=","chunk_sp=","chunk_t=","t_srs="])
  except getopt.GetoptError:
      printHelp()
      sys.exit(2)
      
  for opt, arg in opts:
      if opt == '--help':
	  printHelp()
      elif opt in ("-d", "--dir"):
         directory = arg
      elif opt in ("-a", "--array"):
         array = arg
      elif opt in ("--host","-h"):
         host = arg
         con -= 1
      elif opt in ("--port","-p"):
         port = arg
         con -= 1
      elif opt in ("--user","-u"):
         user = arg
         con -= 1
      elif opt in ("--pwd","-w"):
         pwd = arg
         con -= 1
      elif opt in ("--border"):
	 border = arg
      elif opt in ("--chunk_sp"):
	 ch_sp = arg
      elif opt in ("--chunk_t"):
	 ch_t = arg
      elif opt in ("--t_srs"):
	 t_srs = arg

def multi_delete(list_, args):
    indexes = sorted(args, reverse=True)
    for index in indexes:
        del list_[index]
    return list_

def  printConnectionString():
  global host,port,user,pwd
  if (host is None):
      host = os.environ.get("SCIDB4GDAL_HOST")
  if (port is None):
      port = os.environ.get("SCIDB4GDAL_PORT")
  if (user is None):
      user = os.environ.get("SCIDB4GDAL_USER")
  if (pwd is None):
      pwd = os.environ.get("SCIDB4GDAL_PASSWD")
  
  if (host is None or port is None or user is None or pwd is None):
    sys.exit("Error: stated connection parameters are incomplete")
    
  return "host="+host+" port="+str(port)+" user="+user+" password="+pwd



if __name__ == "__main__":
  gdal.UseExceptions()
  gdal.PushErrorHandler(gdal_error_handler)
  #parse the parameter from the commandline into variables
  parseParameter(sys.argv[1:])
  if (directory is None):
    sys.exit('ERROR: Directory or file is missing.')
  
  if (array is None):
     sys.exit('ERROR: No target array specified.')
  
  if (con > -4):
    if (user is None and os.environ.get("SCIDB4GDAL_USER") is None ):
      #user = os.environ.get("SCIDB4GDAL_USER")
      sys.exit("Error: No user name specified.")
    else: 
      con += 1
    
    if (host is None and os.environ.get("SCIDB4GDAL_HOST") is None):
      sys.exit("Error: No host specified.")
    else: 
      con += 1
      
    if (port is None and os.environ.get("SCIDB4GDAL_PORT") is None):
      sys.exit("Error: No port specified.")
    else: 
      con += 1
      
    if (pwd is None and os.environ.get("SCIDB4GDAL_PASSWD") is None):
      sys.exit("Error: No password stated.")
    else: 
      con += 1


 
  version_num = int(gdal.VersionInfo('VERSION_NUM'))
  if version_num < 1100000:
      sys.exit('ERROR: Python bindings of GDAL 1.10 or later required')
  
  error_count=0

  # install error handler
  

  # Raise a dummy error (first is the error type, second the numbers of errors
  #gdal.Error(2, 2, 'test error')

  #if (directory is None):
      #gdal.Error(1,++error_count,"No directory set, using default for testing")
      #directory = "/home/lahn/GitHub/scidbgdal/test/chunksize/ls-files/"


  file_dir = []
  raster_images = []
  for f in os.listdir(directory):
      path = join(directory,f)
      path_cap,ending = os.path.splitext(path)
      
      if (ending == ".bak"):
	  continue
      try:
	  img = gdal.Open(path)
      except:
	  continue
	
      if (not(img is None)):
	file_dir.append(path)
	raster_images.append(img)
      else:
	gdal.Error(1,++error_count, "No driver for \""+path+"\" found. Skipping file.")
	continue

  
  #raster_images = [ for path in file_dir]

  left = right = top = bottom = None

  authority = code = None
  interval = raster_images[0].GetMetadataItem("TINTERVAL")

  cleaning = []
  #get the largest extent for setting up the scidb array and check meta data of the images
  for img in raster_images:
      proj = img.GetProjectionRef()
      src = osr.SpatialReference()
      src.ImportFromWkt(proj)
      s_auth= src.GetAttrValue("authority",0)
      s_code= src.GetAttrValue("authority", 1)

      if (authority is None and code is None):
	if (not(t_srs is None)):
	    authority,code=t_srs.split(":")
	else:
	    authority = s_auth
	    code = s_code
	    
	
      if (s_auth != authority or s_code != code) :

	gdal.Error(1, ++error_count, "The images do not have the same spatial reference system. Attempting to transform the image into "+authority+":"+code)
	
	#either the image was already warped in a prior run, in which case we just need to neglect this current image or we need warp it
	p = warp(file_dir[raster_images.index(img)],authority,code)
	if (not(p is None)):
	    file_dir.append(p)
	    raster_images.append(gdal.Open(p))    
	
	index = raster_images.index(img)
	cleaning.append(index)
	continue


      if (img.GetMetadataItem("TIMESTAMP") is None):
	gdal.Error(3, ++error_count, "An image does not have a time stamp meta data tag.")
      if (img.GetMetadataItem("TINTERVAL") is None):
	gdal.Error(3, ++error_count, "An image does not have a temporal interval meta data tag.")
      elif (img.GetMetadataItem("TINTERVAL") != interval):
	gdal.Error(3, ++error_count, "One or more images have a different temporal interval meta data entry.")
      
      cols = img.RasterXSize
      rows = img.RasterYSize
      t = img.GetGeoTransform()
      coords = GetExtent(t,cols,rows)
      #print coords
      if (left is None):
	  left = min(coords[0][0], coords[1][0])
      elif (coords[0][0] < left or coords[1][0] < left):
	  left = min(coords[0][0], coords[1][0])
      if (right is None):
	  right = max(coords[2][0], coords[3][0])
      elif (coords[2][0] > right or coords[3][0] > right):
	  right = max(coords[2][0], coords[3][0])
      if (top is None):
	  top = max(coords[0][1], coords[3][1])
      elif (coords[0][1] > top or coords[3][1] > top):
	  top = max(coords[0][1], coords[3][1])
      if (bottom is None):
	  bottom = min(coords[1][1], coords[2][1])
      elif (coords[1][1] < bottom or coords[2][1] < bottom):
	  bottom = min(coords[1][1], coords[2][1])
  
  file_dir = multi_delete(file_dir,cleaning)
  raster_images=multi_delete(raster_images,cleaning)
  #round values because GDAL driver has problems with rounding the boundaries, which results in errors
  if (border is None):
    border=0.001 #0.1% border
    
  left = left - (border*(right-left))
  bottom = bottom - (border*(top-bottom))
  right = right + (border*(right-left))
  top = top + (border*(top-bottom))

  #os.system("gdalinfo "+file_dir[0])

  #performing the upload of the data
  useConStr=(con<0)
  #useConStr=True
  
  for i in range(0,len(raster_images),1) :
      command=[]
      
      command.append("gdal_translate")
      command.append("-of")
      command.append("SciDB")
      
      if (i == 0):
	  #createSTSstr = "gdal_translate -of SciDB -co type=STS"
	  #command += createSTSstr.split(" ")
	  #createSTSstr = "-co \"bbox="+str(left)+" "+ str(bottom) + " "+ str(right) + " "+ str(top)+"\" "
	  
	  
	  command.append("-co")
	  command.append("type=STS")
	  command.append("-co")
	  command.append("bbox="+str(left)+" "+ str(bottom) + " "+ str(right) + " "+ str(top))

	  
	  #createSTSstr = "-co srs="+authority+":"+str(code)+" "
	  #createSTSstr += "-co t="+raster_images[i].GetMetadataItem("TIMESTAMP")+" -co dt="+raster_images[i].GetMetadataItem("TINTERVAL")+" "
	  command.append("-co")
	  command.append("srs="+authority+":"+str(code))
	  
	  
	  
      else:
	  #createSTSstr = "gdal_translate -of SciDB -co type=ST "
	  #command.append("gdal_translate")
	  #command.append("-of SciDB")
	  command.append("-co")
	  command.append("type=ST")
	  
	  #createSTSstr += "-co t="+raster_images[i].GetMetadataItem("TIMESTAMP")+" -co dt="+raster_images[i].GetMetadataItem("TINTERVAL")+" "

      
      command.append("-co")
      command.append("t="+raster_images[i].GetMetadataItem("TIMESTAMP"))
      command.append("-co")
      command.append("dt="+raster_images[i].GetMetadataItem("TINTERVAL"))
      
      if (not (ch_sp is None)):
	#createSTSstr += "-co CHUNKSIZE_SP="+ch_sp+" "
	command.append("-co")
	command.append("CHUNKSIZE_SP="+ch_sp)
      if (not (ch_t is None)):
	#createSTSstr += "-co CHUNKSIZE_T="+ch_t+" "
	command.append("-co")
	command.append("CHUNKSIZE_T="+ch_t)
      
      #createSTSstr += file_dir[i]+" "
      #command += createSTSstr.split(" ")
      command.append(file_dir[i])
      
      if (useConStr):
	#createSTSstr +=  "\"SCIDB:array="+array+" "+printConnectionString()+"\""
	command.append("SCIDB:array="+array+" "+printConnectionString())
      else:
	#createSTSstr +=  "\"SCIDB:array="+array+"\""
	command.append("SCIDB:array="+array)
	
      
      
      print command
      call(command)
      
      #time.sleep(1)
      print "Image \t"+str(i+1)+" of "+str(len(raster_images))+" uploaded."
      #sys.stdout.write("\rUploading: %d%%" % perc)
      #sys.stdout.flush()

 # print ""
  #print authority+":"+code
  #uninstall error handler
  print "Image batch succesfully uploaded"
  gdal.PopErrorHandler()