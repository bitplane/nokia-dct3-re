-- Physical Numbers/Add, PIN2, A/12345 on the enabled FDN card.
_G.nsb3_fdn_record_steps = {
    {77, 'Scroll Down'}, {78, 'Left Softkey / Menu'},
    {79, 'Keypad 5'}, {79.5, 'Keypad 6'}, {80, 'Keypad 7'},
    {80.5, _G.nsb3_fdn_record_wrong and 'Keypad 9' or 'Keypad 8'}, {81, 'Left Softkey / Menu'},
    {82, 'Keypad 2'}, {83, 'Left Softkey / Menu'},
    {84, 'Keypad 1'}, {84.5, 'Keypad 2'}, {85, 'Keypad 3'},
    {85.5, 'Keypad 4'}, {86, 'Keypad 5'}, {86.5, 'Left Softkey / Menu'}}
if _G.nsb3_fdn_record_wrong then
    while #_G.nsb3_fdn_record_steps > 7 do table.remove(_G.nsb3_fdn_record_steps) end
end
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsb3_fdn_records_input.lua')
