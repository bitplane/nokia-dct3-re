-- Cold-use the PIN2 installed by PUK2 recovery, then change back to the default.
_G.npe3_pin2_old='2468'
_G.npe3_pin2_new='5678'
local directory=assert(debug.getinfo(1,'S').source:match('^@(.*/)'))
dofile(directory..'noki6210_pin2_change_input.lua')
