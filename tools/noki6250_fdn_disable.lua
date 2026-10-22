_G.nhm3_fdn_toggle=true
_G.nhm3_fdn_disable=true
local directory=assert(debug.getinfo(1,'S').source:match('^@(.*/)'))
dofile(directory..'noki6250_fdn.lua')
