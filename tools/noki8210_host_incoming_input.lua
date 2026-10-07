local source = debug.getinfo(1, 'S').source:sub(2)
_G.noki8210_host_incoming = true
dofile(assert(source:match('^(.*[/])')) .. 'noki8210_incoming_call_input.lua')
