_G.nsb7_puk2_wrong=true
local source=debug.getinfo(1,'S').source:sub(2)
dofile(assert(source:match('^(.*[/\\])'))..'nsb7_puk2_recover.lua')
