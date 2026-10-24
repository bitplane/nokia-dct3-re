-- Verify the deleted Outbox in another cold process, without rewriting it.
_G.nsb3_sms_steps = {
    {34, 'Left Softkey / Menu'}, {37, 'Left Softkey / Menu'},
    {40, 'Scroll Down'}, {43, 'Left Softkey / Menu'}}
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsb3_sms_input.lua')
