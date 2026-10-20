_G.nsm1_pin2_old={8,7,6,5,4,3,2,2}
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsm1_pin2_change.lua')
