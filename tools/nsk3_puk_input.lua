_G.nsk3_puk_recovery=true
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsk3_pin_input.lua')
