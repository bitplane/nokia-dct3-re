_G.nsb7_correct_pin=true
local source=debug.getinfo(1,'S').source:sub(2)
dofile(assert(source:match('^(.*[/\\])'))..'nsb7_pin_input.lua')
