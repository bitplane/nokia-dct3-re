_G.nsk1_fdn_unlisted=true
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsk1_fdn_dial.lua')
