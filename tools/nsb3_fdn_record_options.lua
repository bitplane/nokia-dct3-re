-- Physical Search/Details/Options and PIN2; firmware owns credential commands.
_G.nsb3_fdn_fast_navigation = true
_G.nsb3_fdn_record_steps = {
    {54, 'Left Softkey / Menu'}, {56, 'Left Softkey / Menu'},
    {58, 'Left Softkey / Menu'}, {60, 'Left Softkey / Menu'},
    {62, 'Keypad 5'}, {62.5, 'Keypad 6'}, {63, 'Keypad 7'},
    {63.5, 'Keypad 8'}, {64, 'Left Softkey / Menu'}}
for _, step in ipairs(_G.nsb3_fdn_option_steps or {}) do
    _G.nsb3_fdn_record_steps[#_G.nsb3_fdn_record_steps + 1] = step
end
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsb3_fdn_records_input.lua')
