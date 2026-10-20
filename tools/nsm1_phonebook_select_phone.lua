_G.nsm1_select_phone=true
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsm1_phonebook_memory.lua')
