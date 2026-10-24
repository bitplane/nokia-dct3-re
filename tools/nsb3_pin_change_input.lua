-- Physical Access codes navigation; the firmware owns credential commands.
_G.nsb3_security_steps = {
    {40, 'Scroll Down'}, {43, 'Scroll Down'}, {46, 'Scroll Down'},
    {49, 'Left Softkey / Menu'}, {52, 'Scroll Down'}, {55, 'Left Softkey / Menu'},
    {58, 'Scroll Down'}, {61, 'Scroll Down'}, {64, 'Scroll Down'}, {67, 'Scroll Down'},
    {70, 'Scroll Down'}, {73, 'Left Softkey / Menu'}, {76, 'Left Softkey / Menu'}}
_G.nsb3_security_settle = 0.2
local when = 79
for _, digits in ipairs({'1234', '4321', '4321'}) do
    for digit in digits:gmatch('.') do
        _G.nsb3_security_steps[#_G.nsb3_security_steps + 1] = {when, 'Keypad ' .. digit}
        when = when + 0.5
    end
    _G.nsb3_security_steps[#_G.nsb3_security_steps + 1] = {when, 'Left Softkey / Menu'}
    when = when + 1.5
end
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsb3_security_input.lua')
