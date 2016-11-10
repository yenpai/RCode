PDIR := $(realpath ./)
TDIR := $(realpath ../)

#############################################################
# Expected Variables 
#   SUBDIRS  - all subdirs with a Makefile
#   CSRCS    - all "C" files in the dir
#   GEN_LIBS - list of libs to be generated
#   GEN_BINS - list of binaries to be generated
#   COMPONENTS_xxx - a list of libs/objs to be extracted and 
#                    rolled up into a generated libxxx.a
#############################################################

GEN_BINS = MyServer
COMPONENTS_MyServer = \
	$(TDIR)/EDLoop/libEDLoop.a \
	$(TDIR)/Container/libContainer.a \
	$(TDIR)/CLogger/libCLogger.a

#############################################################
# Compile Variables
#############################################################

INCLUDES := -I$(TDIR)/EDLoop -I$(TDIR)/CLogger
INCLUDES += $(DBUS_INCLUDES)

#############################################################
# Recursion Magic
#############################################################

include $(TDIR)/rule.mk
