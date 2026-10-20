-- Verify the replacement established by ordinary CHANGE CHV.
_G.nsk1_replacement_pin=true
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsk1_pin_reload.lua')
