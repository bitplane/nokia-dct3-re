_G.nsb1_pin2_wrong=true
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsb1_pin2_change.lua')
