-- Authenticate the installed PIN2 after cold boot and change it back physically.
_G.npe3_pin2_old='8765'
_G.npe3_pin2_new='5678'
local directory=assert(debug.getinfo(1,'S').source:match('^@(.*/)'))
dofile(directory..'noki6210_pin2_change_input.lua')
