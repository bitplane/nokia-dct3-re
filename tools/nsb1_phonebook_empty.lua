_G.nsb1_empty_phonebook = true
local source = debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])')..'nsb1_phonebook_erase.lua')
