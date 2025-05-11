############################################
# SPDX-License-Identifier: MIT             #
# Copyright (C) 2021-.... Jing Leng        #
# Contact: Jing Leng <lengjingzju@163.com> #
############################################

PACKAGE_NAME    = ljson

majorver       := $(shell cat json.h | grep JSON_VERSION | sed 's/.*0x//g' | cut -b 1-2 | sed 's/^0//g')
minorver       := $(shell cat json.h | grep JSON_VERSION | sed 's/.*0x//g' | cut -b 3-4 | sed 's/^0//g')
patchver       := $(shell cat json.h | grep JSON_VERSION | sed 's/.*0x//g' | cut -b 5-6 | sed 's/^0//g')

staticlib      := libljson.a
sharedlib      := libljson.so $(majorver) $(minorver) $(patchver)
testedbin      := ljson
testednum      := jnum_test

INSTALL_HEADERS = json.h jnum.h
FMUL           ?= 0
HIGH           ?= 1
SMALL          ?= 0
DTOA           ?= 0

libsrcs        := json.c jnum.c
ifeq ($(DTOA),2)
libsrcs        += grisu2.c
endif
ifeq ($(DTOA),3)
libsrcs        += dragonbox.c
endif

CPFLAGS        += -DUSING_FLOAT_MUL=$(FMUL)
CPFLAGS        += -DUSING_HIGH_RESOLUTION=$(HIGH) # resolutions is: 0, 14~15bits; 1, 15~16bits
CPFLAGS        += -DUSING_SMALL_LUT=$(SMALL) # When set to 1, use a lut of half size
CPFLAGS        += -DJSON_DTOA_ALGORITHM=$(DTOA) # 0:ldouble 1:sprintf 2:grisu2 3:dragonbox
CXXFLAGS       += -Wno-missing-field-initializers -Wno-write-strings

.PHONY: all clean install
all:
	@echo "Build $(PACKAGE_NAME) Done!"

INC_MAKES      := app
object_byte_size=10240
include inc.makes

$(eval $(call add-liba-build,$(staticlib),$(libsrcs)))
$(eval $(call add-libso-build,$(sharedlib),$(libsrcs),-lm))
$(eval $(call add-bin-build,$(testedbin),json_test.c,-L $(OBJ_PREFIX) $(call set_links,ljson,m),,$(OBJ_PREFIX)/$(staticlib)))

numsrcs        := jnum.c grisu2.c dragonbox.c jnum_test.c
$(eval $(call add-bin-build,$(testednum),$(numsrcs),-lm))

all: $(BIN_TARGETS) $(LIB_TARGETS)

clean: clean_objs
	@rm -f $(LIB_TARGETS) $(BIN_TARGETS)
	@echo "Clean $(PACKAGE_NAME) Done."

pkgconfigdir   := /usr/lib/pkgconfig
INSTALL_PKGCONFIGS := pcfiles/*
$(eval $(call install_obj,pkgconfig))

install: install_hdrs install_libs install_bins install_pkgconfigs
	@sed -i "s/@Version@/$(majorver).$(minorver).$(patchver)/g" $(INS_PREFIX)$(pkgconfigdir)/ljson.pc
	@echo "Install $(PACKAGE_NAME) to $(INS_PREFIX) Done."
