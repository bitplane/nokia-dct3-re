_G.nsb7_fdn_cancel=true
local source=debug.getinfo(1,'S').source:sub(2)
dofile(assert(source:match('^(.*[/\\])'))..'nsb7_fdn_erase.lua')
