#!/bin/bash

# Send a URL of the form secondlife://... to the viewer.
#

URL="$1"

if [ -z "$URL" ]; then
    #echo Usage: $0 secondlife://...
    echo "Usage: $0 [ secondlife://  | hop:// ] ..."
    exit
fi

RUN_PATH=`dirname "$0" || echo .`
cd "${RUN_PATH}"

VIEWER="$PWD/../mikostorm"

# Pass the URL to an already-running viewer instance via D-Bus, else launch a new one.
if [ `pidof do-not-directly-run-mikostorm-bin` ]; then
	exec dbus-send --type=method_call --dest=com.secondlife.ViewerAppAPIService /com/secondlife/ViewerAppAPI com.secondlife.ViewerAppAPI.GoSLURL string:"$URL"
else
	exec "$VIEWER" -url "$URL"
fi

