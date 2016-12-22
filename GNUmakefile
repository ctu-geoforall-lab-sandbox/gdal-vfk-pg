
include ../../../GDALmake.opt

CORE_OBJ = vfkreader.o vfkreaderdb.o vfkreadersqlite.o vfkreaderpg.o \
	vfkdatablock.o vfkdatablockdb.o \
	vfkpropertydefn.o \
	vfkfeature.o vfkfeaturedb.o vfkfeaturesqlite.o \
	vfkproperty.o

OGR_OBJ = ogrvfkdriver.o ogrvfkdatasource.o ogrvfklayer.o

OBJ = $(CORE_OBJ) $(OGR_OBJ)

CPPFLAGS := -I.. -I../..  $(SQLITE_INC) $(PG_INC) $(CPPFLAGS)

default: $(O_OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

$(O_OBJ): ogr_vfk.h vfkreader.h vfkreaderp.h
