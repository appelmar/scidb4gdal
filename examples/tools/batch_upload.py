#!/usr/bin/python

#
# parts were taken from the tutorial at https://pcjericks.github.io/py-gdalogr-cookbook/gdal_general.html
#

import sys, math, getopt, time
try:
    from osgeo import ogr, osr, gdal
    from subprocess import call
except:
    sys.exit('ERROR: cannot find GDAL/OGR modules. Please make sure the GDAL/OGR and their Python bindings are installed on your machine.')
    
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
    print "\t\t--border=\t The size of the border in percent. Default 0.5% = 0.005"
    print ""
    print "\t\t--chunk_sp=\t The spatial chunksize for the target array"
    print ""
    print "\t\t--chunk_t=\t The temporal chunksize for the target array"
    print ""
    print "\t\t--t_srs=\t The target reference system in case the images have different spatial reference systems"
    print ""
    print "\t\t--add=\t\t Boolean value to mark if whether or not the images shall be uploaded into an existing SciDB array."
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
    warped_file = path+warp_append+".tif"
    if (not os.path.isfile(warped_file)):
	warp_command = ["gdalwarp","-t_srs", authority+":"+srs, "-r", "bilinear", "-of", "GTiff", file, warped_file]
	#print ("gdalwarp -t_srs "+authority+":"+srs+" -r bilinear -of GTiff "+file+" "+warped_file)
	try: 
	  call(warp_command)
	except:
	  sys.exit(0)
	return warped_file
    else:
	print warped_file+" already exists. Skip warping."
	return None
    

def parseParameter(argv):
  #make the parsed parameter globally available
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
  global add
  array=None
  directory=None
  host=port=user=pwd=t_srs=None
  con=0
  border=ch_sp=ch_t=None
  add = False
  
  #if no parameters are provided return the help text
  if (len(argv) == 0):
    printHelp()
    sys.exit(2)
  
  # otherwise try to read and assign the function parameter
  try:
      opts, args = getopt.getopt(argv,"h:d:a:p:u:w:",["help=","dir=","array=","host=","port=","user=","pwd=","border=","chunk_sp=","chunk_t=","t_srs=","add="])
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
	 border = float(arg)
      elif opt in ("--chunk_sp"):
	 ch_sp = arg
      elif opt in ("--chunk_t"):
	 ch_t = arg
      elif opt in ("--t_srs"):
	 t_srs = arg
      elif opt in ("--add"):
	 add = True if (arg.lower()=="true" or arg=="1" or arg.lower()=="t" or arg.lower()=="y" or arg.lower()=="yes") else False

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

def getTime(item):
  return item[2]


#
# The main method of this script
#
if __name__ == "__main__":
  gdal.UseExceptions()
  gdal.PushErrorHandler(gdal_error_handler)

  parseParameter(sys.argv[1:])
  if (directory is None):
    sys.exit('ERROR: Directory or file is missing.')
  
  if (array is None):
     sys.exit('ERROR: No target array specified.')
  
  #after the commandline parameter are checked, check environment variables for connection information
  #in case some information are missing. If they are create an error.
  if (con > -4):
    if (user is None and os.environ.get("SCIDB4GDAL_USER") is None ):
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
  
  table=[] #contains tuples with path, img reference, timestamp
  interval = None
  
  # list all files in the directory and try to open them, if GDAL cannot open a file it will be skipped.
  file_list=os.listdir(directory)
  for f in file_list:
      path = join(directory,f)
      path_cap,ending = os.path.splitext(path)
      
      if (ending == ".bak"):
	  continue
      try:
	  img = gdal.Open(path)
      except:
	  continue
	
      if (not(img is None)):
	# check if the temporal information was set and if the temporal interval is the same in all images, if not throw an error
	if (img.GetMetadataItem("TIMESTAMP") is None):
	  gdal.Error(3, ++error_count, "An image does not have a time stamp meta data tag.")
	  
	timestamp=img.GetMetadataItem("TIMESTAMP")
	  
	if (img.GetMetadataItem("TINTERVAL") is None):
	  gdal.Error(3, ++error_count, "An image does not have a temporal interval meta data tag.")
	  
	if (file_list.index(f) == 0):
	  interval=img.GetMetadataItem("TINTERVAL")
	  
	elif (img.GetMetadataItem("TINTERVAL") != interval):
	  gdal.Error(3, ++error_count, "One or more images have a different temporal interval meta data entry.")
	  
	table.append((path,img,timestamp)) 
      else:
	gdal.Error(1,++error_count, "No driver for \""+path+"\" found. Skipping file.")
	continue

  left = right = top = bottom = None

  authority = code = None
  

  cleaning = []
  
  # from the opened images read the metadata and make sure all images have the same spatial and temporal reference
  # in case an image has no similar SRS then use GDAL warp to transform it into a certain SRS
  #get the largest extent for setting up the scidb array and check meta data of the images
  for tupl in table:
      img = tupl[1]
      proj = img.GetProjectionRef()
      src = osr.SpatialReference()
      src.ImportFromWkt(proj)
      s_auth= src.GetAttrValue("authority",0)
      s_code= src.GetAttrValue("authority", 1)
      
      
      # if the user has used parameter --t_srs then use this parameter to derive the target SRS otherwise take the SRS of the first image
      if (authority is None and code is None):
	if (not(t_srs is None)):
	    authority,code=t_srs.split(":")
	else:
	    authority = s_auth
	    code = s_code
	    
      # if the SRS differs from the assigned target SRS, then warp it into the correct one
      if (s_auth != authority or s_code != code) :
	gdal.Error(1, ++error_count, "The images do not have the same spatial reference system. Attempting to transform the image into "+authority+":"+code)
	
	#either the image was already warped in a prior run, in which case we just need to neglect this current image or we need warp it
	p = warp(tupl[0],authority,code)
	if (not(p is None)):
	    t=gdal.Open(p)
	    table.append((p,t,tupl[2]))
	
	index = table.index(tupl)
	cleaning.append(index)
	#if the image needed to be warped skip the bbox creation for this image
	continue
  
  #remove the old files that have been warped  
  table=multi_delete(table,cleaning)
  
  for tupl in table:
      img = tupl[1]
      proj = img.GetProjectionRef()
      src = osr.SpatialReference()
      src.ImportFromWkt(proj)
      s_auth= src.GetAttrValue("authority",0)
      s_code= src.GetAttrValue("authority", 1)
      
      #extract the bbox information
      cols = img.RasterXSize
      rows = img.RasterYSize
      t = img.GetGeoTransform()
      coords = GetExtent(t,cols,rows)
      
      #get the outer boundary
      img_left = min(coords[0][0], coords[1][0],coords[2][0],coords[3][0])
      img_right = max(coords[0][0], coords[1][0],coords[2][0],coords[3][0])
      img_top = max(coords[0][1], coords[1][1],coords[2][1],coords[3][1])
      img_bottom = min(coords[0][1], coords[1][1],coords[2][1],coords[3][1])
      
      if (table.index(tupl) == 0):
	  left = img_left
	  right = img_right
	  top = img_top
	  bottom = img_bottom
      else:
	  left = img_left if (img_left < left) else left
	  right = img_right if (img_right > right) else right
	  top = img_top if (img_top > top) else top
	  bottom = img_bottom if (img_bottom < bottom) else bottom
      
      #end of loop

  table = sorted(table,key=getTime)
  
  #apply some boundary uncertainty to make sure the bbox is not too small by a little amount
  if (border is None):
    border=0.005 #0.5% border
    
  left = left - (border*(right-left))
  bottom = bottom - (border*(top-bottom))
  right = right + (border*(right-left))
  top = top + (border*(top-bottom))
  
  #if the command has got some connection information rather than storing it as environment parameter, then use the connection string method of GDAL
  useConStr=(con<0)
  failedUploads = []
  nofails=0
  
  
  #if the array shall be created fresh (not adding) then remove the old one first
  if (not add):
    command = []
    command.append("gdalmanage")
    command.append("delete")
    connstr = "SCIDB:array="+array+" confirmDelete=Y" if (useConStr) else "SCIDB:array="+array+" "+ printConnectionString() +" confirmDelete=Y"
    command.append(connstr)
    print "Delete array in case it exists."
    call(command)
  
  #iterate over all opened images and formulate the GDAL commands to upload the images
  for i in range(0,len(table),1) :
      command=[]
      
      command.append("gdal_translate")
      command.append("-of")
      command.append("SciDB")
      
      if (i == 0 and not add):
	  command.append("-co")
	  command.append("type=STS")
	  command.append("-co")
	  command.append("bbox="+str(left)+" "+ str(bottom) + " "+ str(right) + " "+ str(top))
	  command.append("-co")
	  command.append("srs="+authority+":"+str(code)) 	  
      else:
	  command.append("-co")
	  command.append("type=ST")
      
      command.append("-co")
      command.append("t="+table[i][1].GetMetadataItem("TIMESTAMP"))
      command.append("-co")
      command.append("dt="+table[i][1].GetMetadataItem("TINTERVAL"))
      
      if (not (ch_sp is None)):
	command.append("-co")
	command.append("CHUNKSIZE_SP="+ch_sp)
      if (not (ch_t is None)):
	command.append("-co")
	command.append("CHUNKSIZE_T="+ch_t)
      
      command.append(table[i][0])
      
      if (useConStr):
	command.append("SCIDB:array="+array+" "+printConnectionString())
      else:
	command.append("SCIDB:array="+array)
      
      print "Uploading image \t"+str(i+1)+" of "+str(len(table))+":"
      if (call(command) <> 0):
	 nofails += 1
	 failedUploads.append(i)
      print "Image \t"+str(i+1)+" of "+str(len(table))+" uploaded."
  
  if (nofails == 0):
      print "Image batch succesfully uploaded"
  else:
      print "Image batch uploaded with errors. "+str(nofails)+" images were not uploaded:\n"
      for index in failedUploads:
	print table[index][0]
      
  gdal.PopErrorHandler()