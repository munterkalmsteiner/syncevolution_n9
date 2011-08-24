#! /bin/sh
#
# Wrapper script which starts a new D-Bus session before
# running a program and kills the D-Bus daemon when done.

# start D-Bus session
eval `dbus-launch`
export DBUS_SESSION_BUS_ADDRESS

# Work-around for GNOME keyring daemon not started
# when accessed via org.freedesktop.secrets: start it
# explicitly.
# See https://launchpad.net/bugs/525642 and
# https://bugzilla.redhat.com/show_bug.cgi?id=572137
/usr/bin/gnome-keyring-daemon --start --foreground --components=secrets &
KEYRING_PID=$!

# kill all programs started by us
trap "kill $KEYRING_PID; kill $DBUS_SESSION_BUS_PID" EXIT

# If DBUS_SESSION_SH_EDS_BASE is set and our main program runs
# under valgrind, then also check EDS. DBUS_SESSION_SH_EDS_BASE
# must be the directory which contains e-addressbook/calendar-factory.
E_CAL_PID=
E_BOOK_PID=
case "$@" in *valgrind*) prefix=`echo $@ | perl -p -e 's;.*?(\S*/?valgrind\S*).*;$1;'`;;
             *) prefix=;;
esac
if [ "$DBUS_SESSION_SH_EDS_BASE" ] && [ "$prefix" ]; then
    $prefix $DBUS_SESSION_SH_EDS_BASE/e-calendar-factory &
    E_CAL_PID=$!
    $prefix $DBUS_SESSION_SH_EDS_BASE/e-addressbook-factory &
    E_BOOK_PID=$!

    # give daemons some time to start and register with D-Bus
    sleep 5
else
    DBUS_SESSION_SH_EDS_BASE=
fi

# run program
"$@"
res=$?
echo dbus-session.sh: program returned $res >&2

# Now also shut down EDS, if started by us. Update total result code if any of them
# failed the valgrind check.
shutdown () {
    pid=$1
    program=$2

    # give process 20 seconds to shut down
    i=0
    while [ "$pid" ] && ps | grep -q "$pid" && [ $i -lt 20 ]; do
	sleep 1
	i=`expr $i + 1`
    done
    if [ "$pid" ] && ps | grep -q "$pid"; then
	kill -9 "$pid" 2>/dev/null
    fi
    wait "$pid"
    subres=$?
    case $subres in 0|130|143) true;; # 130 and 143 indicate that it was killed, probably by us
                    *) echo $program failed with return code $subres >&2
	               if [ $res -eq 0 ]; then
                           res=$subres
                       fi
		       ;;
    esac
}

if [ "$DBUS_SESSION_SH_EDS_BASE" ]; then
    kill "$E_CAL_PID" 2>/dev/null
    kill "$E_BOOK_PID" 2>/dev/null
    shutdown "$E_CAL_PID" "$DBUS_SESSION_SH_EDS_BASE/e-calendar-factory"
    shutdown "$E_BOOK_PID" "$DBUS_SESSION_SH_EDS_BASE/e-addressbook-factory"
fi

echo dbus-session.sh: final result $res >&2
return $res
