_G.nsm1_wrong_puk=true
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsm1_puk_input.lua')
