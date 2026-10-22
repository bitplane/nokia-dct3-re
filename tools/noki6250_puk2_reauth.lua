_G.nhm3_pin2_old='2468'
_G.nhm3_pin2_new='1357'
local directory=assert(debug.getinfo(1,'S').source:match('^@(.*/)'))
dofile(directory..'noki6250_pin2_change.lua')
