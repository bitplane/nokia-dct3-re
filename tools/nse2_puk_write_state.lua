-- Select real UNBLOCK and replacement-PIN entry for the SIM replay observer.
_G.nse2_sim_state_puk=true
local directory=debug.getinfo(1,'S').source:sub(2):match('^(.*[/\\])')
dofile(directory..'nse2_sim_write_state.lua')
