_G.nsb3_fdn_dial_number = '54321'
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsb3_fdn_dial.lua')
