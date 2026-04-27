packet-checker: ${BUILD_BIN_DIR}/packet_checker.vesper
packet-checker: CFLAGS=-I"${ROOT_DIR}/include" -fPIC
packet-checker: LDFLAGS=-shared

${BUILD_BIN_DIR}/packet_checker.vesper: ${PACKET_CHECKER_OBJ}
	@$(call build_log, ${CC} <objects> -o $@)
	@${CC} $^ ${LDFLAGS} -o $@
	@echo Build Successful: $@

packet-checker-install: superuser
	@$(call build_log, "Installing...")
	@${MKDIR_P} ${DESTDIR}/etc/vespercsi/plugins
	@install --target-directory ${DESTDIR}/etc/vespercsi/plugins ${BUILD_BIN_DIR}/packet_checker.vesper

packet-checker-uninstall: superuser
	@$(call build_log, "Uninstalling...")
	@rm -f ${DESTDIR}/etc/vespercsi/plugins/packet_checker.vesper

$(eval $(call autoconf_include,packet-checker))
