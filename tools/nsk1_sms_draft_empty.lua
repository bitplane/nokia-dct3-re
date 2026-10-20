-- Open Outbox on the card resulting from a prior physical deletion.
_G.nsk1_sms_empty = true
local source = debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsk1_sms_draft_reload.lua')
