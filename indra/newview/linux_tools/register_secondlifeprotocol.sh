#!/bin/bash

# Register a protocol handler (default: handle_secondlifeprotocol.sh) for
# URLs of the form secondlife://...
#

HANDLER="$1"

RUN_PATH=`dirname "$0" || echo .`
cd "${RUN_PATH}/.."

if [ -z "$HANDLER" ]; then
    HANDLER=`pwd`/etc/handle_secondlifeprotocol.sh
fi

# Register handler for GNOME-aware apps
LLGCONFTOOL2=gconftool-2
if which ${LLGCONFTOOL2} >/dev/null; then
    (${LLGCONFTOOL2} -s -t string /desktop/gnome/url-handlers/secondlife/command "${HANDLER} \"%s\"" && ${LLGCONFTOOL2} -s -t bool /desktop/gnome/url-handlers/secondlife/enabled true) || echo Warning: Did not register secondlife:// handler with GNOME: ${LLGCONFTOOL2} failed.
else
    echo Warning: Did not register secondlife:// handler with GNOME: ${LLGCONFTOOL2} not found.
fi

# Register handler for KDE-aware apps
for LLKDECONFIG in kde-config kde4-config; do
    if [ `which $LLKDECONFIG` ]; then
        LLKDEPROTODIR=`$LLKDECONFIG --path services | cut -d ':' -f 1`
        if [ -d "$LLKDEPROTODIR" ]; then
            LLKDEPROTOFILE=${LLKDEPROTODIR}/secondlife.protocol
            cat > ${LLKDEPROTOFILE} <<EOF || echo Warning: Did not register secondlife:// handler with KDE: Could not write ${LLKDEPROTOFILE}
[Protocol]
exec=${HANDLER} '%u'
protocol=secondlife
input=none
output=none
helper=true
listing=
reading=false
writing=false
makedir=false
deleting=false
EOF
        else
            echo Warning: Did not register secondlife:// handler with KDE: Directory $LLKDEPROTODIR does not exist.
        fi
    fi
done

# Register handler the modern way (XDG / freedesktop), used by current
# GNOME, Cinnamon, KDE, XFCE, etc. via a .desktop file + xdg-mime.
DESKTOP_INSTALL_DIR="${HOME}/.local/share/applications"
mkdir -p "${DESKTOP_INSTALL_DIR}"
HANDLER_PATH="`pwd`/etc/handle_secondlifeprotocol.sh"
HANDLER_DESKTOP="${DESKTOP_INSTALL_DIR}/mikostorm-slurl-handler.desktop"
cat > "${HANDLER_DESKTOP}" <<EOF
[Desktop Entry]
Type=Application
Name=MikoStorm SLURL Handler
NoDisplay=true
Exec=${HANDLER_PATH} %u
MimeType=x-scheme-handler/secondlife;x-scheme-handler/hop;
EOF
if which xdg-mime >/dev/null 2>&1; then
    xdg-mime default mikostorm-slurl-handler.desktop x-scheme-handler/secondlife x-scheme-handler/hop 2>/dev/null || true
fi
if which update-desktop-database >/dev/null 2>&1; then
    update-desktop-database "${DESKTOP_INSTALL_DIR}" 2>/dev/null || true
fi

