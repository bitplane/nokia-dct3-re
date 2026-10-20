_G.nsm1_pin2_old={5,6,7,5}
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsm1_pin2_change.lua')
