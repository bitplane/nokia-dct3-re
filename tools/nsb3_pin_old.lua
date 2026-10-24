-- The original PIN must fail after the firmware changes it to 4321.
_G.nsb3_pin_correct = false
_G.nsb3_pin_digits = '1234'
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsb3_pin_input.lua')
