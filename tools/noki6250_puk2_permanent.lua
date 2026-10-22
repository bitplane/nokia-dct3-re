-- Reopen the own PIN2 setting after permanent loss; do not enter credentials.
_G.nhm3_pin2_phases={}
_G.nhm3_pin2_finish=50
local directory=assert(debug.getinfo(1,'S').source:match('^@(.*/)'))
dofile(directory..'noki6250_pin2_change.lua')
