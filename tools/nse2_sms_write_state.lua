_G.nse2_sim_state_sms=true
local directory=debug.getinfo(1,'S').source:sub(2):match('^(.*[/\\])')
dofile(directory..'nse2_sim_write_state.lua')
