_G.nsb7_wrong_puk=true
local source=debug.getinfo(1,'S').source:sub(2)
dofile(assert(source:match('^(.*[/\\])'))..'nsb7_puk_input.lua')
