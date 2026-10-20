_G.nsb1_fdn_disable=true
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsb1_fdn_input.lua')
