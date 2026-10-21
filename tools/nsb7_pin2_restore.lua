_G.nsb7_pin2_change=true
_G.nsb7_change_old='8765'
_G.nsb7_change_new='5678'
local source=debug.getinfo(1,'S').source:sub(2)
dofile(assert(source:match('^(.*[/\\])'))..'nsb7_pin_change.lua')
