-- A wrong PIN2 must not activate fixed dialling.
_G.npe3_fdn_enable=true
_G.npe3_fdn_pin2='5675'
local directory=assert(debug.getinfo(1,'S').source:match('^@(.*/)'))
dofile(directory..'noki6210_fdn_input.lua')
