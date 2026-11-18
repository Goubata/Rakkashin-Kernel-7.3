#!/system/bin/sh
case "$(uname -r)" in 7.3.*) ;; *) exit 0 ;; esac
O=$(readlink /data/local/tmp/avr73/latest)
[ -d "$O" ] && echo "$(date) service.sh reached" >> $O/events.txt
