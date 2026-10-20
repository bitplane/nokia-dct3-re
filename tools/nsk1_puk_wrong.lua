-- Incorrect PUK submitted through the ordinary physical unblock editor.
_G.nsk1_wrong_puk = true
local source = debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsk1_puk_input.lua')
