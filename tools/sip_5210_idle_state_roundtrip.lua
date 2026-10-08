-- NSM-5 puts physical Send in column zero.
_G.sip_state_answer_port = ':COL.0'
_G.sip_state_answer_field = 'Send'
local source = debug.getinfo(1, 'S').source:sub(2)
local directory = assert(source:match('^(.*[/])'))
dofile(directory .. 'sip_idle_state_roundtrip.lua')
