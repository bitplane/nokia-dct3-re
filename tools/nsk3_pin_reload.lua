_G.nsk3_correct_pin=true
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsk3_pin_input.lua')
