_G.nsb1_pin2_entry_back=true
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsb1_pin2_entry.lua')
