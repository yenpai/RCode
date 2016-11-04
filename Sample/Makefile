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

GEN_BINS = Sample
COMPONENTS_Sample = \
	$(TDIR)/EDLoop/libEDLoop.a \
	$(TDIR)/Container/libContainer.a \
	$(TDIR)/CLogger/libCLogger.a

#############################################################
# Compile Variables
#############################################################

DBUS_INCLUDES ?= -I/usr/include/dbus-1.0 -I/usr/lib/x86_64-linux-gnu/dbus-1.0/include

INCLUDES := -I$(TDIR)/EDLoop -I$(TDIR)/CLogger
INCLUDES += $(DBUS_INCLUDES)

LINKLIBS += -ldbus-1

#############################################################
# Recursion Magic
#############################################################

include $(TDIR)/rule.mk
