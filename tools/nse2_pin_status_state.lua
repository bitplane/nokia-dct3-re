-- Card committed the physical VERIFY; its status remains queued in SIMI.
_G.nse2_sim_state_pin=true
_G.nse2_sim_state_status_pending=true
local directory=debug.getinfo(1,'S').source:sub(2):match('^(.*[/\\])')
dofile(directory..'nse2_sim_write_state.lua')
