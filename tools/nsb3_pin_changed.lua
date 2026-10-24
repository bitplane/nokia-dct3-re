_G.nsb3_pin_correct = true
_G.nsb3_pin_digits = '4321'
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsb3_pin_input.lua')
