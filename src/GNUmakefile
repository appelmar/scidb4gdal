
include ../../GDALmake.opt

OBJ	=	scidbdriver.o shimclient.o utils.o affinetransform.o tilecache.o

CPPFLAGS	:=	$(GDAL_INCLUDE) $(CPPFLAGS) $(CURL_INC)

default:	$(OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))
