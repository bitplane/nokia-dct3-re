-- Physical Fixed dialing/Numbers entrance on the already enabled lab card.
_G.nsb3_security_steps = {
    {40, 'Scroll Down'}, {43, 'Scroll Down'}, {46, 'Scroll Down'},
    {49, 'Left Softkey / Menu'}, {52, 'Scroll Down'}, {55, 'Left Softkey / Menu'},
    {58, 'Scroll Down'}, {61, 'Scroll Down'}, {64, 'Scroll Down'},
    {67, 'Left Softkey / Menu'}, {70, 'Scroll Down'}, {73, 'Scroll Down'},
    {76, 'Left Softkey / Menu'}}
_G.nsb3_security_settle = 0.2
if _G.nsb3_fdn_fast_navigation then
    for index, step in ipairs(_G.nsb3_security_steps) do step[1] = 39 + index end
end
for _, step in ipairs(_G.nsb3_fdn_record_steps or {}) do
    _G.nsb3_security_steps[#_G.nsb3_security_steps + 1] = step
end
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsb3_security_input.lua')
