-- Physical PIN2 item and credentials; no SIM commands are supplied by Lua.
_G.nsb3_change_pin2 = true
_G.nsb3_change_digits = {'5678', '8765', '8765'}
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsb3_pin_change_input.lua')
