-- Save/load settled lab idle; only physical Menu input after exact replay.
_G.nsb7_security_continue=true
local source=debug.getinfo(1,'S').source:sub(2)
local directory=assert(source:match('^(.*[/\\])'))
dofile(directory..'nsb7_page_observe.lua')
local machine=manager.machine
local capture=dofile(directory..'arm_architecture_snapshot.lua')
local saved,complete
local retained={}
local function snapshot(event)
    local state=capture(machine)
    machine:logerror(string.format('nsb7_state: event=%s pc=%08x sp=%08x ram=%08x cpu=%s t=%.9f\n',
        event,state.pc,state.sp,state.ram,state.cpu,state.time))
    return state
end
retained[#retained+1]=emu.add_machine_pre_save_notifier(function()
    saved=snapshot('saved')
    machine:logerror(string.format('state_replay: phase=reference event=begin t=%.9f\n',saved.time))
end)
retained[#retained+1]=emu.add_machine_post_load_notifier(function()
    local restored=snapshot('restored')
    assert(saved,'save observation absent')
    for _,name in ipairs({'time','pc','sp','ram','cpu'}) do
        assert(saved[name]==restored[name],'idle architectural state mismatch: '..name)
    end
    machine:logerror(string.format('state_roundtrip: result=pass scenario=nsb7_idle requested_at=%.9f t=%.9f\n',saved.time,restored.time))
    machine:logerror(string.format('state_replay: phase=restored event=begin t=%.9f\n',restored.time))
    local replay=coroutine.create(function()
        assert(emu.wait(3))
        machine:logerror(string.format('state_replay: phase=restored event=end t=%.9f\n',machine.time:as_double()))
        machine.screens[':screen']:snapshot('nsb7_state_idle_restored.png')
        local key
        for _,port in pairs(machine.ioport.ports) do
            key=key or port.fields['Left Softkey / Menu']
        end
        assert(key,'Menu contact absent')
        machine:logerror('nsb7_state_physical: key=Menu state=1\n')
        key:set_value(1)
        assert(emu.wait(0.2))
        key:set_value(0)
        machine:logerror('nsb7_state_physical: key=Menu state=0\n')
        assert(emu.wait(0.8))
        machine.screens[':screen']:snapshot('nsb7_state_idle_menu.png')
        complete=true
        machine:exit()
    end)
    retained[#retained+1]=replay
    assert(coroutine.resume(replay))
end)
local runner=coroutine.create(function()
    assert(emu.wait(27))
    machine:save('nsb7_idle')
    assert(emu.wait(3))
    assert(saved,'save did not execute')
    machine:logerror(string.format('state_replay: phase=reference event=end t=%.9f\n',machine.time:as_double()))
    machine.screens[':screen']:snapshot('nsb7_state_idle_reference.png')
    machine:load('nsb7_idle')
end)
retained[#retained+1]=runner
retained[#retained+1]=emu.add_machine_stop_notifier(function()
    if not complete then machine:logerror('nsb7_state: FAIL incomplete\n') end
end)
_G.nsb7_state_retained=retained
assert(coroutine.resume(runner))
