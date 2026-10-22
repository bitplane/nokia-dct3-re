-- Observe multiple ordinary STATUS periods after POLLING OFF; presence
-- detection is independent of proactive polling (GSM 11.14 section 6.4.14).
_G.nsb7_toolkit_menu=true
_G.nsb7_toolkit_network_wait=100
local source=debug.getinfo(1,'S').source:sub(2)
dofile(assert(source:match('^(.*[/\\])'))..'nsb7_toolkit_interactive.lua')
