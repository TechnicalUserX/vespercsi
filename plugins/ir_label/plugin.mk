ir-label: ${BUILD_BIN_DIR}/ir_label.vesper
ir-label: CFLAGS=-I"${ROOT_DIR}/include" -fPIC
ir-label: LDFLAGS=-shared -lgpiod -lconfig

${BUILD_BIN_DIR}/ir_label.vesper: ${IR_LABEL_OBJ}
	@$(call build_log, ${CC} <objects> -o $@)
	@${CC} $^ ${LDFLAGS} -o $@
	@echo Build Successful: $@

ir-label-install: superuser
	@$(call build_log, "Installing...")
	@${MKDIR_P} ${DESTDIR}/etc/vespercsi/plugins
	@install --target-directory ${DESTDIR}/etc/vespercsi/plugins ${BUILD_BIN_DIR}/ir_label.vesper

ir-label-uninstall: superuser
	@$(call build_log, "Uninstalling...")
	@rm -f ${DESTDIR}/etc/vespercsi/plugins/ir_label.vesper

$(eval $(call autoconf_include,ir-label))
