_G.nsk1_pin2_entry_only=true
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsk1_pin2_change.lua')
