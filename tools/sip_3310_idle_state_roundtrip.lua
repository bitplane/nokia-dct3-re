-- NHM-5 uses the Menu/Navi field in column three to answer a call.
_G.sip_state_answer_port = ':COL.3'
_G.sip_state_answer_field = 'Menu'
local source = debug.getinfo(1, 'S').source:sub(2)
local directory = assert(source:match('^(.*[/])'))
dofile(directory .. 'sip_idle_state_roundtrip.lua')
