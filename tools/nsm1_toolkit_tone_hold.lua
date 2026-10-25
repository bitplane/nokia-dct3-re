-- Same cold card and physical menu route, but no Quit contact.
_G.nsm1_toolkit_tone_cancel=false
local directory=debug.getinfo(1,'S').source:sub(2):match('^(.*[/\\])')
dofile(directory..'nsm1_toolkit_tone.lua')
