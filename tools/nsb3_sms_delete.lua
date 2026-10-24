-- Open the persisted unsent draft and its physical Options menu.
_G.nsb3_sms_steps = {
    {34, 'Left Softkey / Menu'}, {37, 'Left Softkey / Menu'},
    {40, 'Scroll Down'}, {43, 'Left Softkey / Menu'},
    {46, 'Left Softkey / Menu'}, {49, 'Left Softkey / Menu'},
    {52, 'Left Softkey / Menu'}, {55, 'Left Softkey / Menu'}}
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsb3_sms_input.lua')
