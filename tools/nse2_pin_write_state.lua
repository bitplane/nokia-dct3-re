-- Select physical wrong-PIN VERIFY for the shared SIM transaction observer.
_G.nse2_sim_state_pin=true
local directory=debug.getinfo(1,'S').source:sub(2):match('^(.*[/\\])')
dofile(directory..'nse2_sim_write_state.lua')
