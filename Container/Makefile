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
GEN_LIBS = libContainer.a

#############################################################
# Compile Variables
#############################################################

#DEFINES  := 
#INCLUDES :=  

#############################################################
# Recursion Magic
#############################################################

include $(TDIR)/rule.mk

