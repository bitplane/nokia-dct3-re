-- Pending external decision: exact CPU/RAM restore, not equal branch traffic.
local source=debug.getinfo(1,'S').source:sub(2)
local directory=assert(source:match('^(.*[/\\])'))
dofile(directory..'nsb7_host_ussd_request.lua')
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
end)
retained[#retained+1]=emu.add_machine_post_load_notifier(function()
    local restored=snapshot('restored')
    assert(saved,'save observation absent')
    for _,name in ipairs({'time','pc','sp','ram','cpu'}) do
        assert(saved[name]==restored[name],'architectural state mismatch: '..name)
    end
    machine:logerror(string.format('state_roundtrip: result=pass scenario=nsb7_ussd requested_at=%.9f t=%.9f\n',saved.time,restored.time))
    local continuation=coroutine.create(function()
        for _,second in ipairs({36,37}) do
            assert(emu.wait(second-machine.time:as_double()))
            machine.screens[':screen']:snapshot('nsb7_ussd_response_'..second..'.png')
        end
        local fields={}
        for _,port in pairs(machine.ioport.ports) do
            for _,field in pairs(port.fields) do fields[field.name]=field end
        end
        for index,action in ipairs({{38,'End'},{40,'Left Softkey / Menu'}}) do
            assert(emu.wait(action[1]-machine.time:as_double()))
            local field=assert(fields[action[2]])
            machine:logerror(string.format('nsb7_ussd_input: key=%s state=1 t=%.9f\n',action[2],machine.time:as_double()))
            field:set_value(1)
            assert(emu.wait(0.2))
            field:set_value(0)
            machine:logerror(string.format('nsb7_ussd_input: key=%s state=0 t=%.9f\n',action[2],machine.time:as_double()))
            assert(emu.wait(0.8))
            machine.screens[':screen']:snapshot('nsb7_ussd_key_'..(index+6)..'.png')
        end
        assert(emu.wait(43-machine.time:as_double()))
        complete=true
        machine:logerror('nsb7_ussd: complete\n')
        machine:exit()
    end)
    retained[#retained+1]=continuation
    assert(coroutine.resume(continuation))
end)
local runner=coroutine.create(function()
    assert(emu.wait(33.8))
    machine:save('nsb7_ussd')
    assert(emu.wait(1))
    assert(saved,'save did not execute')
    machine.screens[':screen']:snapshot('nsb7_ussd_pending_reference.png')
    machine:load('nsb7_ussd')
end)
retained[#retained+1]=runner
retained[#retained+1]=emu.add_machine_stop_notifier(function()
    if not complete then machine:logerror('nsb7_state: FAIL incomplete\n') end
end)
_G.nsb7_ussd_restore_retained=retained
assert(coroutine.resume(runner))
