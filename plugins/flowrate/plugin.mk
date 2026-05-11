flowrate: ${BUILD_BIN_DIR}/flowrate.vesper
flowrate: CFLAGS=-I"${ROOT_DIR}/include" -fPIC
flowrate: LDFLAGS=-shared 

${BUILD_BIN_DIR}/flowrate.vesper: ${FLOWRATE_OBJ}
	@$(call build_log, ${CC} <objects> -o $@)
	@${CC} $^ ${LDFLAGS} -o $@
	@echo Build Successful: $@

flowrate-install: superuser
	@$(call build_log, "Installing...")
	@${MKDIR_P} ${DESTDIR}/etc/vespercsi/plugins
	@install --target-directory ${DESTDIR}/etc/vespercsi/plugins ${BUILD_BIN_DIR}/flowrate.vesper

flowrate-uninstall: superuser
	@$(call build_log, "Uninstalling...")
	@rm -f ${DESTDIR}/etc/vespercsi/plugins/flowrate.vesper

$(eval $(call autoconf_include,flowrate))
