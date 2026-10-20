_G.nsb1_puk_phase = 'cold'
local source = debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])')..'nsb1_puk_input.lua')
