local source = debug.getinfo(1, 'S').source:sub(2)
_G.noki6250_host_incoming = true
dofile(assert(source:match('^(.*[/])')) .. 'noki6250_call_observe.lua')
