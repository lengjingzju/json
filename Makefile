############################################
# SPDX-License-Identifier: MIT             #
# Copyright (C) 2021-.... Jing Leng        #
# Contact: Jing Leng <lengjingzju@163.com> #
############################################

PACKAGE_NAME    = ljson

major_ver       = 1
minor_ver       = 3
patch_ver       = 1
staticlib       = lib$(PACKAGE_NAME).a
sharedlib       = lib$(PACKAGE_NAME).so $(major_ver) $(minor_ver) $(patch_ver)
testedbin       = ljson

ldoublelib      = libldouble.a
ldoublebin      = ldouble

INSTALL_HEADERS = json.h
DTOA           ?= 0
RBIT           ?= 11
CPFLAGS        += -DJSON_DTOA_ALGORITHM=$(DTOA) # 0:ldouble 1:sprintf 2:grisu2 3:dragonbox
CPFLAGS        += -DLSHIFT_RESERVED_BIT=$(RBIT) # 11 is 8% faster than 10, 10 is more similar to sprintf than 11

.PHONY: all clean install
all:
	@echo "Build $(PACKAGE_NAME) Done!"

INC_MAKES      := app
object_byte_size=2304
include inc.makes
$(eval $(call add-liba-build,$(staticlib),json.c))
$(eval $(call add-libso-build,$(sharedlib),json.c))
$(eval $(call add-bin-build,$(testedbin),json_test.c,-static -L $(OBJ_PREFIX) -lljson))
$(OBJ_PREFIX)/$(testedbin): $(OBJ_PREFIX)/$(staticlib)

$(eval $(call add-liba-build,$(ldoublelib),ldouble.c))
$(eval $(call add-bin-build,$(ldoublebin),ldouble_test.c,-static -L $(OBJ_PREFIX) -lldouble))
$(OBJ_PREFIX)/$(ldoublebin): $(OBJ_PREFIX)/$(ldoublelib)

all: $(BIN_TARGETS) $(LIB_TARGETS)

clean: clean_objs
	@rm -f $(LIB_TARGETS) $(BIN_TARGETS)
	@echo "Clean $(PACKAGE_NAME) Done."

install: install_hdrs install_libs install_bins
	@echo "Install $(PACKAGE_NAME) to $(INS_PREFIX) Done!"
