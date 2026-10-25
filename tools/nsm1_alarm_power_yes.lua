-- Same physical alarm lifecycle, choosing activation and authenticating again.
_G.nsm1_alarm_activate_yes=true
local directory=debug.getinfo(1,'S').source:sub(2):match('^(.*[/\\])')
dofile(directory..'nsm1_alarm_power.lua')
