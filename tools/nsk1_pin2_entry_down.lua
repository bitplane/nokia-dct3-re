_G.nsk1_pin2_entry_down=true
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsk1_pin2_entry.lua')
