-- Firmware-owned security-code save, using the handset's physical menu.
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(assert(source:match('^(.*[/])')) .. 'noki8890_clock_read.lua')
local machine = manager.machine
local cpu = assert(machine.devices[':maincpu'])
cpu.debug:bpset(0x2f4d2a, 'r1==709',
    'logerror "8890_code_change: event=persist encoded=%08x caller=%08x\\n",d@r0,r14;g')
local input = coroutine.create(function()
    assert(emu.wait(22))
    local function press(column, name, label)
        machine:logerror('8890_code_change_physical: action=' .. label .. '\n')
        local key = assert(machine.ioport.ports[':COL.' .. column].fields[name])
        key:set_value(1)
        assert(emu.wait(0.15))
        key:set_value(0)
        assert(emu.wait(0.85))
        machine.screens[':screen']:snapshot('8890_code_change_' .. label .. '.png')
    end
    press(1, 'Menu', 'menu')
    for index = 2, 4 do press(1, 'Scroll Down', 'menu_' .. index) end
    press(1, 'Menu', 'settings')
    for index = 2, 6 do press(1, 'Scroll Down', 'settings_' .. index) end
    press(1, 'Menu', 'security')
    for index = 2, 5 do press(1, 'Scroll Down', 'security_' .. index) end
    press(1, 'Menu', 'access_codes')
    for index = 2, 3 do press(1, 'Scroll Down', 'access_' .. index) end
    assert(emu.wait(9))
    press(1, 'Menu', 'old_prompt')
    local keys = {{2, 'Keypad 1'}, {3, 'Keypad 2'}, {4, 'Keypad 3'},
                  {2, 'Keypad 4'}, {3, 'Keypad 5'}}
    for index, item in ipairs(keys) do press(item[1], item[2], 'old_' .. index) end
    press(1, 'Menu', 'new_prompt')
    for index = 5, 1, -1 do press(keys[index][1], keys[index][2], 'new_' .. index) end
    press(1, 'Menu', 'confirm_prompt')
    for index = 5, 1, -1 do press(keys[index][1], keys[index][2], 'confirm_' .. index) end
    press(1, 'Menu', 'result')
    assert(emu.wait(5))
    machine.screens[':screen']:snapshot('8890_code_change_settled.png')
end)
_G.noki8890_code_change_input = input
assert(coroutine.resume(input))
