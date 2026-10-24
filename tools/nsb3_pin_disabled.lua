-- PIN request is disabled in the persisted card, not by this input script.
_G.nsb3_phonecode_start = 20
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsb3_phonecode_input.lua')
