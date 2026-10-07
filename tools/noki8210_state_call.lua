local source = debug.getinfo(1, 'S').source:sub(2)
_G.noki8210_state_call = true
dofile(assert(source:match('^(.*[/])')) .. 'noki8210_state_roundtrip.lua')
