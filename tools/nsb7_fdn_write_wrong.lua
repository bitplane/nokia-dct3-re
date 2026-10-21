_G.nsb7_fdn_wrong_pin2=true
local source=debug.getinfo(1,'S').source:sub(2)
dofile(assert(source:match('^(.*[/\\])'))..'nsb7_fdn_write.lua')
