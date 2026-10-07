-- Read a network-originated SMS through physical softkeys.
local source = debug.getinfo(1, 'S').source:sub(2)
local directory = assert(source:match('^(.*[/])'))
_G.noki8850_security_only = true
dofile(directory .. 'noki8850_radio_observe.lua')
dofile(directory .. 'noki8850_sms_read.lua')
