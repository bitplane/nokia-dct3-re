_G.nhm3_fdn_pin2='5675'
local directory=assert(debug.getinfo(1,'S').source:match('^@(.*/)'))
dofile(directory..'noki6250_fdn_enable.lua')
