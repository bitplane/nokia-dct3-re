local source = debug.getinfo(1, 'S').source:sub(2)
_G.noki5210_pin_phonebook_read = true
dofile(assert(source:match('^(.*[/])')) .. 'noki5210_pin_phonebook.lua')
