_G.nhm3_pin2_old='8765'
_G.nhm3_pin2_new='2468'
local directory=assert(debug.getinfo(1,'S').source:match('^@(.*/)'))
dofile(directory..'noki6250_pin2_change.lua')
