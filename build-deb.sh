#!/bin/bash
set -e

VERSION="1.15.26"
PKG_NAME="mikostorm"
BUILD_DIR="build-linux-x86_64/newview"
PACKAGED_DIR="${BUILD_DIR}/packaged"
DEB_BUILD_DIR="/tmp/${PKG_NAME}-deb"

echo "Building ${PKG_NAME} ${VERSION} .deb..."

# Clean
rm -rf "${DEB_BUILD_DIR}"
mkdir -p "${DEB_BUILD_DIR}/opt/${PKG_NAME}"
mkdir -p "${DEB_BUILD_DIR}/DEBIAN"

# Copy packaged content to /opt/mikostorm/
cp -a "${PACKAGED_DIR}/"* "${DEB_BUILD_DIR}/opt/${PKG_NAME}/"

# Fix wrapper script to use /opt/mikostorm path
sed -i "s|RUN_PATH=\$(dirname.*|RUN_PATH=\"/opt/${PKG_NAME}\"|" "${DEB_BUILD_DIR}/opt/${PKG_NAME}/mikostorm"

# Create control file
cat > "${DEB_BUILD_DIR}/DEBIAN/control" << EOF
Package: ${PKG_NAME}
Version: ${VERSION}
Section: net
Priority: optional
Architecture: amd64
Depends: libgl1-mesa-glx | libgl1, libglu1-mesa-glx | libglu1, libx11-6, libxrandr2, libxinerama1, libfreetype6, libfontconfig1, libpulse0, libglib2.0-0, libstdc++6 (>= 12), libgstreamer1.0-0, libgstreamer-plugins-base1.0-0
Recommends: libopenal1, libvulkan1, mesa-vulkan-drivers
Installed-Size: $(du -sk "${DEB_BUILD_DIR}/opt/${PKG_NAME}" | cut -f1)
Maintainer: MikoStorm <noreply@github.com/DDynamic-Evolution/MikoStorm>
Homepage: https://github.com/DDynamic-Evolution/MikoStorm
Description: Client for 3D virtual worlds
 MikoStorm is a third-party viewer for Second Life and OpenSim-based
 virtual worlds. It is a fork of the Firestorm Viewer with additional
 features and customizations.
EOF

# Create postinst
cat > "${DEB_BUILD_DIR}/DEBIAN/postinst" << 'POSTINST'
#!/bin/bash
set -e
if command -v gtk-update-icon-cache >/dev/null 2>&1; then
    gtk-update-icon-cache -f -t /usr/share/icons/hicolor || true
fi
if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database /usr/share/applications || true
fi
POSTINST
chmod 755 "${DEB_BUILD_DIR}/DEBIAN/postinst"

# Create prerm
cat > "${DEB_BUILD_DIR}/DEBIAN/prerm" << 'PRERM'
#!/bin/bash
set -e
rm -f /usr/share/applications/mikostorm.desktop || true
if command -v gtk-update-icon-cache >/dev/null 2>&1; then
    gtk-update-icon-cache -f -t /usr/share/icons/hicolor || true
fi
if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database /usr/share/applications || true
fi
PRERM
chmod 755 "${DEB_BUILD_DIR}/DEBIAN/prerm"

# Create postrm
cat > "${DEB_BUILD_DIR}/DEBIAN/postrm" << 'POSTRM'
#!/bin/bash
set -e
if [ "$1" = "remove" ] || [ "$1" = "purge" ]; then
    rm -rf /opt/mikostorm || true
fi
POSTRM
chmod 755 "${DEB_BUILD_DIR}/DEBIAN/postrm"

# Install desktop file
mkdir -p "${DEB_BUILD_DIR}/usr/share/applications"
cat > "${DEB_BUILD_DIR}/usr/share/applications/mikostorm.desktop" << EOF
[Desktop Entry]
Type=Application
Name=MikoStorm Viewer
GenericName=Virtual World Client
Comment=Client for 3D virtual worlds
Exec=/opt/${PKG_NAME}/mikostorm %u
Icon=/opt/${PKG_NAME}/mikostorm_icon.png
Terminal=false
Categories=Network;Internet;
MimeType=x-scheme-handler/hop;x-scheme-handler/secondlife;
StartupNotify=true
StartupWMClass=do-not-directly-run-mikostorm-bin
EOF

# Set permissions
find "${DEB_BUILD_DIR}/opt/${PKG_NAME}" -type d -exec chmod 755 {} \;
find "${DEB_BUILD_DIR}/opt/${PKG_NAME}" -type f -exec chmod 644 {} \;
find "${DEB_BUILD_DIR}/opt/${PKG_NAME}" -type f -name "*.so*" -exec chmod 755 {} \;
chmod 755 "${DEB_BUILD_DIR}/opt/${PKG_NAME}/mikostorm"
chmod 755 "${DEB_BUILD_DIR}/opt/${PKG_NAME}/bin/do-not-directly-run-mikostorm-bin"
chmod 755 "${DEB_BUILD_DIR}/opt/${PKG_NAME}/bin/SLPlugin" 2>/dev/null || true
chmod 755 "${DEB_BUILD_DIR}/opt/${PKG_NAME}/bin/linux-crash-logger.bin" 2>/dev/null || true
chmod 755 "${DEB_BUILD_DIR}/opt/${PKG_NAME}/bin/chrome-sandbox" 2>/dev/null || true
chmod 755 "${DEB_BUILD_DIR}/opt/${PKG_NAME}/bin/dullahan_host" 2>/dev/null || true

# Build .deb
OUTPUT_FILE="${BUILD_DIR}/${PKG_NAME}_${VERSION}_amd64.deb"
dpkg-deb --root-owner-group --build "${DEB_BUILD_DIR}" "${OUTPUT_FILE}"

echo "Done: ${OUTPUT_FILE}"
echo "Size: $(du -h "${OUTPUT_FILE}" | cut -f1)"
