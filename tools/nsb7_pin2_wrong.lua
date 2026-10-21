_G.nsb7_pin2_change=true
_G.nsb7_change_old='1111'
local source=debug.getinfo(1,'S').source:sub(2)
dofile(assert(source:match('^(.*[/\\])'))..'nsb7_pin_change.lua')
