-- Cold verification of the PIN established by physical PUK recovery.
_G.nsk1_replacement_pin = true
local source = debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsk1_pin_reload.lua')
