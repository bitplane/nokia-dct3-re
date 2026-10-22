-- A wrong current PIN2 must not install the requested replacement.
_G.npe3_pin2_old='9999'
local directory=assert(debug.getinfo(1,'S').source:match('^@(.*/)'))
dofile(directory..'noki6210_pin2_change_input.lua')
