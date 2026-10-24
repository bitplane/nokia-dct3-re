-- Physical Settings/Security/Fixed dialing; firmware owns CHV/file commands.
_G.nsb3_security_steps = {
    {40, 'Scroll Down'}, {43, 'Scroll Down'}, {46, 'Scroll Down'},
    {49, 'Left Softkey / Menu'}, {52, 'Scroll Down'}, {55, 'Left Softkey / Menu'},
    {58, 'Scroll Down'}, {61, 'Scroll Down'}, {64, 'Scroll Down'},
    {67, 'Left Softkey / Menu'},
    {70, _G.nsb3_fdn_disable and 'Scroll Down' or 'Scroll Up'}, {73, 'Left Softkey / Menu'}}
_G.nsb3_security_settle = 0.2
for index, key in ipairs({'Keypad 5', 'Keypad 6', 'Keypad 7',
        _G.nsb3_fdn_wrong and 'Keypad 9' or 'Keypad 8', 'Left Softkey / Menu'}) do
    _G.nsb3_security_steps[#_G.nsb3_security_steps + 1] = {75.5 + index * 0.5, key}
end
if _G.nsb3_fdn_observe then
    while #_G.nsb3_security_steps > 9 do table.remove(_G.nsb3_security_steps) end
end
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsb3_security_input.lua')
