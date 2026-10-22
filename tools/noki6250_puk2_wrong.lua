_G.nhm3_pin2_phases={{50,'99999999'},{62,'2468'},{70,'2468'}}
_G.nhm3_pin2_finish=80
local directory=assert(debug.getinfo(1,'S').source:match('^@(.*/)'))
dofile(directory..'noki6250_pin2_change.lua')
