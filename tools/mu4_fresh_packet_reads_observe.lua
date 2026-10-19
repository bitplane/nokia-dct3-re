-- Preserve the default ten-second oracle; fresh recordings get an explicit window.
_G.mu4_packet_window_last = 12.0
local source = debug.getinfo(1,'S').source:sub(2)
dofile(assert(source:match('^(.*[/])')) .. 'mu4_packet_reads_observe.lua')
