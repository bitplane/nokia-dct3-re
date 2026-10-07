local source = debug.getinfo(1, 'S').source:sub(2)
_G.noki6210_host_incoming = true
dofile(assert(source:match('^(.*[/])')) .. 'noki6210_incoming_call_input.lua')
