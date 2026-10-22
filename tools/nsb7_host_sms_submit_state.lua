-- Preserve a pending submission; only the external host decides its outcome.
_G.nsb7_sms_reply_continue=true
local source=debug.getinfo(1,'S').source:sub(2)
local directory=assert(source:match('^(.*[/\\])'))
dofile(directory..'nsb7_pin_sms_reply.lua')
local machine=manager.machine
local capture=dofile(directory..'arm_architecture_snapshot.lua')
local fields={}
for _,port in pairs(machine.ioport.ports) do
    for _,field in pairs(port.fields) do fields[field.name]=field end
end
local saved,complete
local retained={}
local function snapshot(event)
    local value=capture(machine)
    machine:logerror(string.format('nsb7_sms_submit_state: event=%s pc=%08x sp=%08x ram=%08x cpu=%s t=%.9f\n',
        event,value.pc,value.sp,value.ram,value.cpu,value.time))
    machine.screens[':screen']:snapshot('nsb7_sms_submit_'..event..'.png')
    return value
end
retained[#retained+1]=emu.add_machine_pre_save_notifier(function() saved=snapshot('saved') end)
retained[#retained+1]=emu.add_machine_post_load_notifier(function()
    local restored=snapshot('restored')
    assert(saved,'save observation absent')
    for _,name in ipairs({'time','pc','sp','ram','cpu'}) do
        assert(saved[name]==restored[name],'architectural state mismatch: '..name)
    end
    machine:logerror('state_roundtrip: result=pass scenario=nsb7_sms_submit\n')
    -- Loading cancels the original Reply coroutine's remaining captures.
    local cleanup=coroutine.create(function()
        for _,target in ipairs({49,53,74}) do
            assert(emu.wait(target-machine.time:as_double()))
            machine.screens[':screen']:snapshot('nsb7_sms_submit_'..target..'.png')
        end
        machine:logerror('nsb7_sms_reply_done: t=74.000000000\n')
        for _,step in ipairs({{78,'End'},{82,'Left Softkey / Menu'},{85,'End'}}) do
            assert(emu.wait(step[1]-machine.time:as_double()))
            local field=assert(fields[step[2]])
            machine:logerror(string.format('nsb7_sms_recovery: key=%s state=1 t=%.9f\n',step[2],machine.time:as_double()))
            field:set_value(1)
            assert(emu.wait(0.2))
            field:set_value(0)
            machine:logerror(string.format('nsb7_sms_recovery: key=%s state=0 t=%.9f\n',step[2],machine.time:as_double()))
            assert(emu.wait(0.8))
            machine.screens[':screen']:snapshot('nsb7_sms_recovery_'..step[1]..'.png')
        end
        assert(emu.wait(88-machine.time:as_double()))
        machine.screens[':screen']:snapshot('nsb7_sms_recovery_final.png')
        machine:logerror('nsb7_sms_recovery: done t=88.000000000\n')
        complete=true
        machine:exit()
    end)
    retained[#retained+1]=cleanup
    assert(coroutine.resume(cleanup))
end)
local runner=coroutine.create(function()
    assert(emu.wait(48))
    machine:save('nsb7_sms_submit')
    assert(emu.wait(0.5))
    assert(saved,'save did not execute')
    machine:load('nsb7_sms_submit')
end)
retained[#retained+1]=runner
retained[#retained+1]=emu.add_machine_stop_notifier(function()
    if not complete then machine:logerror('nsb7_sms_submit_state: FAIL incomplete\n') end
end)
_G.nsb7_sms_submit_state=retained
assert(coroutine.resume(runner))
