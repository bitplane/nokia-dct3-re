-- Physical dial-permission probe; no registration or call-result injection.
local steps = {{40, 'Right Softkey / C'}}
local digits = _G.nsb3_fdn_dial_number or '12346'
for index = 1, #digits do
    steps[#steps + 1] = {41 + index, 'Keypad ' .. digits:sub(index, index)}
end
steps[#steps + 1] = {48, 'Send'}
_G.nsb3_security_steps = steps
_G.nsb3_security_settle = 0.2
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsb3_security_input.lua')
