_G.nsm1_fdn_disable=true
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsm1_fdn_enable.lua')
