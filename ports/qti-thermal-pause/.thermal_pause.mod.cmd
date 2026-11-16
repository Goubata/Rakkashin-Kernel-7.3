savedcmd_thermal_pause.mod := printf '%s\n'   thermal_pause.o | awk '!x[$$0]++ { print("./"$$0) }' > thermal_pause.mod
