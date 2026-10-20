-- Matched control: accept phone code but do not dial or press Call/End.
_G.nsk1_radio_no_call = true
local source = debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsk1_radio_observe.lua')
