udp-send: ${BUILD_BIN_DIR}/udp_send.vesper
udp-send: CFLAGS=-I"${ROOT_DIR}/include" -fPIC
udp-send: LDFLAGS=-shared

${BUILD_BIN_DIR}/udp_send.vesper: ${UDP_SEND_OBJ}
	@$(call build_log, ${CC} <objects> -o $@)
	@${CC} $^ ${LDFLAGS} -o $@
	@echo Build Successful: $@

udp-send-install: superuser
	@$(call build_log, "Installing...")
	@${MKDIR_P} ${DESTDIR}${PREFIX}/etc/vespercsi/plugins
	@install --target-directory ${DESTDIR}${PREFIX}/etc/vespercsi/plugins ${BUILD_BIN_DIR}/udp_send.vesper

udp-send-uninstall: superuser
	@$(call build_log, "Uninstalling...")
	@rm -f ${DESTDIR}${PREFIX}/etc/vespercsi/plugins/udp_send.vesper

$(eval $(call autoconf_include,udp-send))
