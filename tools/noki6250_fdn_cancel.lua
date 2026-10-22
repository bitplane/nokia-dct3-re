_G.nhm3_fdn_cancel=true
local directory=assert(debug.getinfo(1,'S').source:match('^@(.*/)'))
dofile(directory..'noki6250_fdn_erase.lua')
