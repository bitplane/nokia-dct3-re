_G.nsk1_wrong_puk2=true
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsk1_puk2_recover.lua')
