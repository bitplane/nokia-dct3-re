_G.nsb7_sip_restore_incoming=true
local source=debug.getinfo(1,'S').source:sub(2)
dofile(assert(source:match('^(.*[/\\])'))..'nsb7_sip_restore.lua')
