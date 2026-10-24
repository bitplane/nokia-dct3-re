_G.nsb3_change_pin2 = true
_G.nsb3_change_digits = {_G.nsb3_puk2_wrong and '87654322' or '87654321', '8765', '8765'}
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsb3_pin_change_input.lua')
