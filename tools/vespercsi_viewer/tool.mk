vespercsi-viewer: ${BUILD_BIN_DIR}/vespercsi_viewer
vespercsi-viewer: CFLAGS=$(shell pkg-config --cflags gtk4 glib-2.0)
vespercsi-viewer: LDFLAGS=$(shell pkg-config --libs gtk4 glib-2.0)

${BUILD_BIN_DIR}/vespercsi_viewer: ${VESPERCSI_VIEWER_OBJ}
	@$(call build_log, ${CC} <objects> -o $@)
	@${CC} $^ ${LDFLAGS} -o $@
	@echo Build Successful: $@

vespercsi-viewer-install: superuser
	@$(call build_log, "Installing...")
	@install --target-directory ${DESTDIR}${PREFIX}/bin ${BUILD_BIN_DIR}/vespercsi_viewer

vespercsi-viewer-uninstall: superuser
	@$(call build_log, "Uninstalling...")
	@rm -f ${DESTDIR}${PREFIX}/bin/vespercsi_viewer

$(eval $(call autoconf_include,vespercsi-viewer))
