-- Own NSM-3 input: GET INKEY is completed by digit entry and physical OK.
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(assert(source:match('^(.*[/])')) .. 'noki8210_toolkit_input.lua')
local machine = manager.machine
local input = coroutine.create(function()
    assert(emu.wait(37))
    machine.screens[':screen']:snapshot('8210_toolkit_inkey.png')
    for _, item in ipairs({{3, 'Keypad 5', 'inkey_5'}, {1, 'Menu', 'inkey_confirm'}}) do
        local key = assert(machine.ioport.ports[':COL.' .. item[1]].fields[item[2]])
        machine:logerror('8210_toolkit_physical: action=' .. item[3] .. '\n')
        key:set_value(1)
        if not emu.wait(0.15) then key:set_value(0); return end
        key:set_value(0)
        assert(emu.wait(0.85))
    end
    assert(emu.wait(2))
    machine.screens[':screen']:snapshot('8210_toolkit_inkey_complete.png')
end)
_G.noki8210_toolkit_inkey = input
assert(coroutine.resume(input))
