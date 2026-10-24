-- Physical Access codes navigation; the firmware owns credential commands.
_G.nsb3_security_steps = {
    {40, 'Scroll Down'}, {43, 'Scroll Down'}, {46, 'Scroll Down'},
    {49, 'Left Softkey / Menu'}, {52, 'Scroll Down'}, {55, 'Left Softkey / Menu'},
    {58, 'Scroll Down'}, {61, 'Scroll Down'}, {64, 'Scroll Down'}, {67, 'Scroll Down'},
    {70, 'Scroll Down'}, {73, 'Left Softkey / Menu'}}
if _G.nsb3_change_pin2 then
    _G.nsb3_security_steps[#_G.nsb3_security_steps + 1] = {76, 'Scroll Down'}
    _G.nsb3_security_steps[#_G.nsb3_security_steps + 1] = {77, 'Left Softkey / Menu'}
else
    _G.nsb3_security_steps[#_G.nsb3_security_steps + 1] = {76, 'Left Softkey / Menu'}
end
_G.nsb3_security_settle = 0.2
local when = 79
for _, digits in ipairs(_G.nsb3_change_digits or {'1234', '4321', '4321'}) do
    for digit in digits:gmatch('.') do
        _G.nsb3_security_steps[#_G.nsb3_security_steps + 1] = {when, 'Keypad ' .. digit}
        when = when + 0.5
    end
    _G.nsb3_security_steps[#_G.nsb3_security_steps + 1] = {when, 'Left Softkey / Menu'}
    when = when + 1.5
end
for _, step in ipairs(_G.nsb3_change_extra_steps or {}) do
    _G.nsb3_security_steps[#_G.nsb3_security_steps + 1] = step
end
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsb3_security_input.lua')
