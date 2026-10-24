_G.nsb3_puk_wrong = true
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsb3_puk_input.lua')
