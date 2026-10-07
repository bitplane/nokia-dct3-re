-- Use the product's physical Send key; the common fixture owns save/load.
_G.sip_state_answer_port = ':COL.4'
_G.sip_state_answer_field = 'Send'
local source = debug.getinfo(1, 'S').source:sub(2)
local directory = assert(source:match('^(.*[/])'))
dofile(directory .. 'sip_idle_state_roundtrip.lua')
