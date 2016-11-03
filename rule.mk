#############################################################
# Rule Makefile
#############################################################
# Update List:
# 2016/10/22 V1.0 <robin.chen@wondalink.com>
# - First version
#############################################################

#############################################################
# Compile Tools
#############################################################

CROSS 	?=
AR      := ${CROSS}ar
NM      := ${CROSS}nm
CC 		:= ${CROSS}gcc
CPP		:= ${CROSS}g++
OBJCOPY := ${CROSS}objcopy
OBJDUMP := ${CROSS}objdump

MKDIR   := mkdir -p
RM 		:= rm -f
CP      := cp -f

#############################################################
# Compile Path and Target
#############################################################

TDIR    ?= $(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))
PDIR 	?= $(shell pwd)
CSRCS   ?= $(wildcard *.c)
SUBDIRS ?= $(patsubst %/,%,$(dir $(wildcard */Makefile)))

RDIR    ?= $(TDIR)/release
ODIR    := .output
OBJODIR := $(ODIR)/obj
LIBODIR := $(ODIR)/lib
BINODIR := $(ODIR)/bin
OLIBS   := $(GEN_LIBS:%=$(LIBODIR)/%)
OBINS   := $(GEN_BINS:%=$(BINODIR)/%)
OBJS    := $(CSRCS:%.c=$(OBJODIR)/%.o)
DEPS    := $(CSRCS:%.c=$(OBJODIR)/%.d)

#############################################################
# Complile Flags
#############################################################

CCFLAGS       ?= -g -Wall
CFLAGS        ?= $(strip $(CCFLAGS) $(EXTRA_CFLAGS) $(INCLUDES))
DFLAGS        ?= $(strip $(LDFLAGS) $(EXTRA_LDFLAGS) $(LINKLIBS))
INCLUDES      := -I$(TDIR)/include $(INCLUDES)
LINKLIBS      := -lpthread $(LINKLIBS)

EXTRACT_DIR   := $(ODIR)/extract
EXTRACT_UPDIR ?= ../../..

#############################################################
# Functions
#############################################################

define ShortcutRule
$(1): .subdirs $(2)/$(1)
endef

define MakeLibrary
DEP_LIBS_$(1) = $$(foreach lib,$$(filter %.a,$$(COMPONENTS_$(1))),$$(dir $$(lib))$$(LIBODIR)/$$(notdir $$(lib)))
DEP_OBJS_$(1) = $$(foreach obj,$$(filter %.o,$$(COMPONENTS_$(1))),$$(dir $$(obj))$$(OBJODIR)/$$(notdir $$(obj)))
$$(LIBODIR)/$(1).a: $$(OBJS) $$(DEP_OBJS_$(1)) $$(DEP_LIBS_$(1)) $$(DEPENDS_$(1))
	@mkdir -p $$(LIBODIR)
	@$$(if $$(filter %.a,$$?),mkdir -p $$(EXTRACT_DIR)/$(1))
	@$$(if $$(filter %.a,$$?),cd $$(EXTRACT_DIR)/$(1); $$(foreach lib,$$(filter %.a,$$?),$$(AR) xo $$(EXTRACT_UPDIR)/$$(lib);))
	$$(AR) rcs $$@ $$(filter %.o,$$?) $$(if $$(filter %.a,$$?),$$(EXTRACT_DIR)/$(1)/*.o)
##	@$$(if $$(filter %.a,$$?),$$(RM) -r $$(EXTRACT_DIR)/$(1))
endef

define MakeBinary
DEP_LIBS_$(1) = $$(foreach lib,$$(filter %.a,$$(COMPONENTS_$(1))),$$(dir $$(lib))$$(LIBODIR)/$$(notdir $$(lib)))
DEP_OBJS_$(1) = $$(foreach obj,$$(filter %.o,$$(COMPONENTS_$(1))),$$(dir $$(obj))$$(OBJODIR)/$$(notdir $$(obj)))
$$(BINODIR)/$(1): $$(OBJS) $$(DEP_OBJS_$(1)) $$(DEP_LIBS_$(1)) $$(DEPENDS_$(1))
	@mkdir -p $$(BINODIR)
	$$(CC) $$(OBJS) $$(DEP_OBJS_$(1)) $$(DEP_LIBS_$(1)) $$(DFLAGS) -o $$(BINODIR)/$(1)
endef

#############################################################
# Rules base
#############################################################

all: .subdirs $(OBJS) $(OLIBS) $(OBINS)

clean:
	$(foreach sub, $(SUBDIRS), $(MAKE) -C $(sub) clean;)
	$(RM) -r $(ODIR)

release_clean:
	$(RM) -r $(RDIR)

release:
	$(foreach sub, $(SUBDIRS), $(MAKE) -C $(sub) release;)
	@$(if $(GEN_LIBS), echo "Release $(GEN_LIBS) to $(RDIR)/lib"; $(MKDIR) $(RDIR)/lib; cd $(LIBODIR); $(CP) $(GEN_LIBS) $(RDIR)/lib;)
	@$(if $(GEN_BINS), echo "Release $(GEN_BINS) to $(RDIR)/bin"; $(MKDIR) $(RDIR)/bin; cd $(BINODIR); $(CP) $(GEN_BINS) $(RDIR)/bin;)

.subdirs:
	@set -e; $(foreach sub, $(SUBDIRS), $(MAKE) -C $(sub);)

$(OBJODIR)/%.o: %.c
	@$(MKDIR) $(OBJODIR);
	$(CC) $(if $(findstring $<,$(DSRCS)),$(DFLAGS),$(CFLAGS)) $(COPTS_$(*F)) -o $@ -c $<

$(OBJODIR)/%.d: %.c
	@$(MKDIR) $(OBJODIR);
	@echo DEPEND: $(CC) -M $(CFLAGS) $<
	@set -e; rm -f $@; \
	$(CC) -M $(CFLAGS) $< > $@.$$$$; \
	sed 's,\($*\.o\)[ :]*,$(OBJODIR)/\1 $@ : ,g' < $@.$$$$ > $@; \
	$(RM) $@.$$$$

$(foreach lib,$(GEN_LIBS),$(eval $(call ShortcutRule,$(lib),$(LIBODIR))))
$(foreach bin,$(GEN_BINS),$(eval $(call ShortcutRule,$(bin),$(BINODIR))))
$(foreach lib,$(GEN_LIBS),$(eval $(call MakeLibrary,$(basename $(lib)))))
$(foreach bin,$(GEN_BINS),$(eval $(call MakeBinary,$(basename $(bin)))))

