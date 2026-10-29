#!/bin/zsh
# Makes the wireless calculator always ready on this Mac: three background services that start at login and restart if they stop.
#   calcnet  waits for the bridge board (TCP 7878) and makes ~/.calc-port      log: /tmp/calcnet.log
#   boardlog the board's own messages (UDP 9999)                              log: /tmp/board.log
#   calcd    starts the right Mac program for what the calculator runs        log: /tmp/calcd.log
# Usage: ./install_services.sh            install (or reinstall after an update) and start them
#        ./install_services.sh remove     stop and remove them
#        CALCD_ARGS="--control" ./install_services.sh    pass options to calcd (see calcd.py)
set -e
here=${0:A:h}
agents=~/Library/LaunchAgents
python=$(command -v python3)
domain=gui/$(id -u)
names=(calcnet boardlog calcd)

for name in $names; do
  launchctl bootout $domain/com.calc.$name 2>/dev/null || true
  rm -f $agents/com.calc.$name.plist
done
[[ $1 == remove ]] && { echo "removed"; exit 0; }

typeset -A args
args[calcnet]="calcnet.py listen"
args[boardlog]="boardlog.py /tmp/board.log"
args[calcd]="calcd.py ${CALCD_ARGS:-}"
mkdir -p $agents
for name in $names; do
  items="<string>$python</string><string>-u</string>"
  for word in ${(z)args[$name]}; do items+="<string>$word</string>"; done
  cat > $agents/com.calc.$name.plist <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
  <key>Label</key><string>com.calc.$name</string>
  <key>ProgramArguments</key><array>$items</array>
  <key>WorkingDirectory</key><string>$here</string>
  <key>EnvironmentVariables</key><dict><key>PATH</key><string>/opt/homebrew/bin:/usr/local/bin:/usr/bin:/bin</string></dict>
  <key>RunAtLoad</key><true/>
  <key>KeepAlive</key><true/>
  <key>ThrottleInterval</key><integer>5</integer>
  <key>StandardOutPath</key><string>/tmp/$name.log</string>
  <key>StandardErrorPath</key><string>/tmp/$name.log</string>
</dict></plist>
PLIST
  launchctl bootstrap $domain $agents/com.calc.$name.plist
done
echo "running: ${names[*]}. Logs: /tmp/calcnet.log /tmp/board.log /tmp/calcd.log"
