_G.nsk3_wrong_puk=true
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsk3_puk_input.lua')
