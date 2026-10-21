_G.nsb7_correct_pin=true
_G.nsb7_pin_digits='4321'
local source=debug.getinfo(1,'S').source:sub(2)
dofile(assert(source:match('^(.*[/\\])'))..'nsb7_pin_input.lua')
