_G.nsb7_security_with_pin=true
_G.nsb7_menu_base_observer='nsb7_lapdm_observe.lua'
local source=debug.getinfo(1,'S').source:sub(2)
dofile(assert(source:match('^(.*[/\\])'))..'nsb7_phonebook_read_input.lua')
