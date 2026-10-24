-- Physical PIN/phone-code authentication and notification readback only.
_G.noki8850_security_only = true
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(source:match('^(.*[/])') .. 'noki8850_radio_observe.lua')
local machine = manager.machine
local function replay()
    if not emu.wait(34 - machine.time:as_double()) then return end
    machine.screens[':screen']:snapshot('8850_ussd_pending.png')
    for index, action in ipairs(_G.nsm2_host_ussd_actions or
                               {{35, 1, 'Menu'}, {38, 0, 'End'}, {41, 1, 'Menu'}}) do
        if not emu.wait(action[1] - machine.time:as_double()) then return end
        local key = assert(machine.ioport.ports[':COL.' .. action[2]].fields[action[3]])
        machine:logerror(string.format('8850_host_ussd_input: key=%s state=1 t=%.9f\n', action[3], machine.time:as_double()))
        key:set_value(1)
        local continued = emu.wait(0.2)
        key:set_value(0)
        if not continued then return end
        machine:logerror(string.format('8850_host_ussd_input: key=%s state=0 t=%.9f\n', action[3], machine.time:as_double()))
        if not emu.wait(0.8) then return end
        machine.screens[':screen']:snapshot('8850_ussd_action_' .. index .. '.png')
    end
    machine:logerror('8850_host_ussd: complete\n')
end
_G.nsm2_host_ussd_replay = replay
local runner = coroutine.create(function()
    if not emu.wait(30) then return end
    machine.screens[':screen']:snapshot('8850_ussd_initial.png')
    print('8850_host_ussd: ready')
    io.stdout:flush()
    replay()
end)
_G.nsm2_host_ussd_notify = runner
assert(coroutine.resume(runner))
