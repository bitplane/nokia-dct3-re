-- Exact PIN-authenticated state restoration and physical continuation.
local source = debug.getinfo(1, 'S').source:sub(2)
local directory = assert(source:match('^(.*[/])'))
local machine = manager.machine
local call = _G.noki5210_pin_state_call == true
local outgoing = _G.noki5210_pin_state_outgoing == true
local sms = _G.noki5210_pin_state_sms == true
local cpu = assert(machine.devices[':maincpu'])
local observe = dofile(directory .. 'dct3_state_snapshot.lua')(machine, cpu, '5210_pin_state')
local saved, completed
local retained = {}
retained[#retained + 1] = emu.add_machine_pre_save_notifier(function()
    saved = observe('saved')
    if _G.noki5210_pin_state_sip then machine:logerror('sip_state: saved\n') end
end)
retained[#retained + 1] = emu.add_machine_post_load_notifier(function()
    local restored = observe('restored')
    if _G.noki5210_pin_state_sip then machine:logerror('sip_state: restored\n') end
    assert(saved, 'save observation absent')
    for index = 1, 4 do
        assert(saved[index] == restored[index], 'PIN architectural state mismatch')
    end
    machine:logerror('5210_pin_state: result=pass\n')
    if call then machine:logerror('state_roundtrip: result=pass scenario=5210_pin_call\n') end
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
        if sms then
            press('Left Softkey / Menu')
            assert(emu.wait(1.2))
            press('Left Softkey / Menu')
            assert(emu.wait(2.5))
            machine.screens[':screen']:snapshot('5210_pin_sms_read.png')
            completed = true
            return
        end
        if not call then
            press('Left Softkey / Menu')
            assert(emu.wait(2))
            machine.screens[':screen']:snapshot('5210_pin_state_menu.png')
        end
        if _G.noki5210_pin_state_media then assert(emu.wait(3)) end
        press('End')
        assert(emu.wait(call and 8 or 2))
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
_G.noki5210_pin_after_accept = function(press)
    assert(emu.wait(10))
    if _G.noki5210_pin_state_host_incoming then
        machine.screens[':screen']:snapshot('5210_pin_host_idle.png')
        assert(emu.wait(5))
    end
    if call then
        if outgoing then
            _G.noki5210_pin_gap = 0.35
            for digit in ('5551234'):gmatch('.') do press('Keypad ' .. digit) end
        end
        press('Send')
        assert(emu.wait(outgoing and 6 or 3))
    end
    local name = outgoing and '5210_pin_outgoing_call' or call and '5210_pin_call' or sms and '5210_pin_sms' or '5210_pin_idle'
    machine:save(name)
    assert(emu.wait(1))
    assert(saved, 'save did not execute')
    machine.screens[':screen']:snapshot('5210_pin_state_reference.png')
    machine:load(name)
end
dofile(directory .. 'noki5210_pin_late_input.lua')
