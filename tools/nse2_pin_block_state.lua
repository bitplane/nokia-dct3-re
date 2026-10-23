-- Select the actual last-PIN-retry predecessor for the shared VERIFY observer.
_G.nse2_pin_state_retries=1
local directory=debug.getinfo(1,'S').source:sub(2):match('^(.*[/\\])')
dofile(directory..'nse2_pin_write_state.lua')
