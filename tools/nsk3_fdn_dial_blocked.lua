_G.nsk3_fdn_dial_blocked=true
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsk3_fdn_dial.lua')
