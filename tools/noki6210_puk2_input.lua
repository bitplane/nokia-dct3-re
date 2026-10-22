-- Recover PIN2 using the laboratory card's PUK2 through physical Settings input.
_G.npe3_pin2_phases={{50,_G.npe3_puk2 or '87654321'},{62,'2468'},{70,'2468'}}
_G.npe3_pin2_finish=80
local directory=assert(debug.getinfo(1,'S').source:match('^@(.*/)'))
dofile(directory..'noki6210_pin2_change_input.lua')
