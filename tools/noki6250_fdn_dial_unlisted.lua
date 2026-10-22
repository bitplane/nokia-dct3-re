_G.nhm3_fdn_dial_digits='12346'
local directory=assert(debug.getinfo(1,'S').source:match('^@(.*/)'))
dofile(directory..'noki6250_fdn_dial.lua')
