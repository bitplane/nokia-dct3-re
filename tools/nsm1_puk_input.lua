_G.nsm1_puk_recovery=true
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsm1_pin_input.lua')
