_G.nsb7_fdn_unlisted=true
local source=debug.getinfo(1,'S').source:sub(2)
dofile(assert(source:match('^(.*[/\\])'))..'nsb7_fdn_dial.lua')
