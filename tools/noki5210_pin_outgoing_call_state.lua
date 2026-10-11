local source = debug.getinfo(1, 'S').source:sub(2)
_G.noki5210_pin_state_call = true
_G.noki5210_pin_state_outgoing = true
dofile(assert(source:match('^(.*[/])')) .. 'noki5210_pin_state_roundtrip.lua')
