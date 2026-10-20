_G.nsb1_fdn_dial_unlisted=true
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsb1_fdn_dial.lua')
