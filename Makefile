############################################
# SPDX-License-Identifier: MIT             #
# Copyright (C) 2021-.... Jing Leng        #
# Contact: Jing Leng <lengjingzju@163.com> #
############################################

PACKAGE_NAME    = ljson

major_ver       = 1
minor_ver       = 0
patch_ver       = 0
staticlib       = lib$(PACKAGE_NAME).a
sharedlib       = lib$(PACKAGE_NAME).so $(major_ver) $(minor_ver) $(patch_ver)
testedbin       = ljson_test

INSTALL_HEADERS = json.h

.PHONY: all clean install
all:

INC_MAKES      := app
include inc.makes
$(eval $(call add-liba-build,$(staticlib),json.c,-lm))
$(eval $(call add-libso-build,$(sharedlib),json.c,-lm))
$(eval $(call add-bin-build,$(testedbin),json_test.c,-static -L $(OBJ_PREFIX) -lljson -lm))

all: $(BIN_TARGETS) $(LIB_TARGETS)

$(BIN_TARGETS): $(LIB_TARGETS)

clean: clean_objs
	@rm -f $(LIB_TARGETS) $(BIN_TARGETS)
	@echo "Clean $(PACKAGE_NAME) Done."

install: install_hdrs install_libs install_bins
