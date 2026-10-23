-- Save the physical VERIFY body while five bytes remain buffered in SIMI.
_G.nse2_sim_state_pin=true
_G.nse2_sim_state_buffered=true
local directory=debug.getinfo(1,'S').source:sub(2):match('^(.*[/\\])')
dofile(directory..'nse2_sim_write_state.lua')
