-- Cancel the card's request through the prompt's physical right softkey.
_G.nsb7_toolkit_decline=true
local source=debug.getinfo(1,'S').source:sub(2)
dofile(assert(source:match('^(.*[/\\])'))..'nsb7_toolkit_call.lua')
