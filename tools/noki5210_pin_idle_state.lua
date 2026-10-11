-- Restore PIN-authenticated idle, then continue through physical Menu/End.
local source = debug.getinfo(1, 'S').source:sub(2)
local directory = assert(source:match('^(.*[/])'))
local machine = manager.machine
local cpu = assert(machine.devices[':maincpu'])
local observe = dofile(directory .. 'dct3_state_snapshot.lua')(machine, cpu, '5210_pin_state')
local saved, completed
local retained = {}
retained[#retained + 1] = emu.add_machine_pre_save_notifier(function()
    saved = observe('saved')
end)
retained[#retained + 1] = emu.add_machine_post_load_notifier(function()
    local restored = observe('restored')
    assert(saved, 'save observation absent')
    for index = 1, 4 do
        assert(saved[index] == restored[index], 'PIN idle architectural state mismatch')
    end
    machine:logerror('5210_pin_state: result=pass\n')
    local replay = coroutine.create(function()
        assert(emu.wait(1))
        machine.screens[':screen']:snapshot('5210_pin_state_restored.png')
        local function press(name)
            for _, port in pairs(machine.ioport.ports) do
                local field = port.fields[name]
                if field then
                    machine:logerror('5210_pin_state_physical: key=' .. name .. '\n')
                    field:set_value(1)
                    assert(emu.wait(0.15))
                    field:set_value(0)
                    return
                end
            end
            error('missing physical key ' .. name)
        end
        press('Left Softkey / Menu')
        assert(emu.wait(2))
        machine.screens[':screen']:snapshot('5210_pin_state_menu.png')
        press('End')
        assert(emu.wait(2))
        machine.screens[':screen']:snapshot('5210_pin_recovered.png')
        completed = true
    end)
    retained[#retained + 1] = replay
    assert(coroutine.resume(replay))
end)
retained[#retained + 1] = emu.add_machine_stop_notifier(function()
    machine:logerror('5210_pin_state: completed=' .. tostring(completed == true) .. '\n')
end)
_G.noki5210_pin_state_retained = retained
_G.noki5210_pin_after_accept = function()
    assert(emu.wait(10))
    machine:save('5210_pin_idle')
    assert(emu.wait(1))
    assert(saved, 'save did not execute')
    machine.screens[':screen']:snapshot('5210_pin_state_reference.png')
    machine:load('5210_pin_idle')
end
dofile(directory .. 'noki5210_pin_late_input.lua')
