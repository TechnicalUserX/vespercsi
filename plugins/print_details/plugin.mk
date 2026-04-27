print-details: ${BUILD_BIN_DIR}/print_details.vesper
print-details: CFLAGS=-I"${ROOT_DIR}/include" -fPIC
print-details: LDFLAGS=-shared

${BUILD_BIN_DIR}/print_details.vesper: ${PRINT_DETAILS_OBJ}
	@$(call build_log, ${CC} <objects> -o $@)
	@${CC} $^ ${LDFLAGS} -o $@
	@echo Build Successful: $@

print-details-install: superuser
	@$(call build_log, "Installing...")
	@${MKDIR_P} ${DESTDIR}/etc/vespercsi/plugins
	@install --target-directory ${DESTDIR}/etc/vespercsi/plugins ${BUILD_BIN_DIR}/print_details.vesper

print-details-uninstall: superuser
	@$(call build_log, "Uninstalling...")
	@rm -f ${DESTDIR}/etc/vespercsi/plugins/print_details.vesper

$(eval $(call autoconf_include,print-details))
