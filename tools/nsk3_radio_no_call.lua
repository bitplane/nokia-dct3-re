_G.nsk3_radio_call=false
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsk3_radio_input.lua')
