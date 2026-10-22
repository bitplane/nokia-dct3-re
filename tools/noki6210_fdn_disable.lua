-- Disable card-owned fixed dialling using physical PIN2 input.
_G.npe3_fdn_disable=true
local directory=assert(debug.getinfo(1,'S').source:match('^@(.*/)'))
dofile(directory..'noki6210_fdn_input.lua')
