-- Matched physical control: omit only the final Stop contact.
_G.nsm1_clock_keep_alarm=true
local directory=debug.getinfo(1,'S').source:sub(2):match('^(.*[/\\])')
dofile(directory..'nsm1_clock_alarm.lua')
