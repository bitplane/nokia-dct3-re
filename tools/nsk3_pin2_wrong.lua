_G.nsk3_pin2_old={5,6,7,5}
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsk3_pin2_change.lua')
