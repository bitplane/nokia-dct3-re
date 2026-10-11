-- Save after CP acknowledgment, before the handset-owned RP timer expires.
local source = debug.getinfo(1, 'S').source:sub(2)
local directory = assert(source:match('^(.*[/])'))
local machine = manager.machine
local observe = dofile(directory .. 'dct3_state_snapshot.lua')(
    machine, assert(machine.devices[':maincpu']), '5210_pin_state')
local retained, saved, continuation = {}, nil, nil
retained[#retained + 1] = emu.add_machine_pre_save_notifier(function()
    saved = observe('saved')
end)
retained[#retained + 1] = emu.add_machine_post_load_notifier(function()
    local restored = observe('restored')
    assert(saved, 'save observation absent')
    for index = 1, 4 do
        assert(saved[index] == restored[index], 'SMS RP-wait architecture mismatch')
    end
    machine:logerror('5210_pin_state: result=pass\n')
    local replay = coroutine.create(function()
        assert(emu.wait(1))
        machine.screens[':screen']:snapshot('5210_pin_state_restored.png')
        continuation()
        machine:logerror('5210_pin_state: completed=true\n')
    end)
    retained[#retained + 1] = replay
    assert(coroutine.resume(replay))
end)
_G.noki5210_pin_sms_state_retained = retained
_G.noki5210_pin_sms_save_wait = function(press, snapshot, finish)
    assert(emu.wait(5))
    continuation = function() finish(press, snapshot, 54) end
    machine:save('5210_pin_sms_wait')
    assert(emu.wait(1))
    assert(saved, 'save did not execute')
    machine.screens[':screen']:snapshot('5210_pin_state_reference.png')
    machine:load('5210_pin_sms_wait')
end
dofile(directory .. 'noki5210_pin_sms_silence.lua')
