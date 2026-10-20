_G.nsm1_pin2_old={8,7,6,5}
_G.nsm1_pin2_new={5,6,7,8}
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsm1_pin2_change.lua')
