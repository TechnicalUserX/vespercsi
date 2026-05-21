livestd: ${BUILD_BIN_DIR}/livestd.vesper
livestd: CFLAGS=-I"${ROOT_DIR}/include" -fPIC
livestd: LDFLAGS=-shared -lm

${BUILD_BIN_DIR}/livestd.vesper: ${LIVESTD_OBJ}
	@$(call build_log, ${CC} <objects> -o $@)
	@${CC} $^ ${LDFLAGS} -o $@
	@echo Build Successful: $@

livestd-install: superuser
	@$(call build_log, "Installing...")
	@${MKDIR_P} ${DESTDIR}/etc/vespercsi/plugins
	@install --target-directory ${DESTDIR}/etc/vespercsi/plugins ${BUILD_BIN_DIR}/livestd.vesper

livestd-uninstall: superuser
	@$(call build_log, "Uninstalling...")
	@rm -f ${DESTDIR}/etc/vespercsi/plugins/livestd.vesper

$(eval $(call autoconf_include,livestd))
