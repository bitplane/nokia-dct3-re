-- Physical NPE-3 Calculator navigation and arithmetic; no internal UI events.
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(assert(source:match('^(.*[/])')) .. 'noki6210_staged_observe.lua')
local machine = manager.machine
local input = coroutine.create(function()
    if not emu.wait(20) then return end
    local function press(column, name, action)
        local key = assert(machine.ioport.ports[':COL.' .. column].fields[name])
        machine:logerror('6210_application_physical: action=' .. action .. '\n')
        key:set_value(1)
        if not emu.wait(0.15) then key:set_value(0); return false end
        key:set_value(0)
        if not emu.wait(0.85) then return false end
        machine.screens[':screen']:snapshot('6210_calculator_' .. action .. '.png')
        return true
    end
    local sequence = {
        {1, 'Left Softkey / Menu', 'messages'},
        {1, 'Scroll Down', 'menu_2'}, {1, 'Scroll Down', 'menu_3'},
        {1, 'Scroll Down', 'menu_4'}, {1, 'Scroll Down', 'menu_5'},
        {1, 'Scroll Down', 'menu_6'}, {1, 'Scroll Down', 'menu_7'},
        {1, 'Left Softkey / Menu', 'application'},
        {2, 'Keypad 1', 'input_1'}, {3, 'Keypad 2', 'input_12'},
        {1, 'Left Softkey / Menu', 'operation_options'},
        {1, 'Scroll Down', 'subtract'}, {1, 'Left Softkey / Menu', 'minus'},
        {4, 'Keypad 3', 'input_3'}, {1, 'Left Softkey / Menu', 'options'},
        {1, 'Left Softkey / Menu', 'result'},
    }
    for _, step in ipairs(sequence) do
        if not press(step[1], step[2], step[3]) then return end
    end
end)
_G.noki6210_application_input = input
assert(coroutine.resume(input))
