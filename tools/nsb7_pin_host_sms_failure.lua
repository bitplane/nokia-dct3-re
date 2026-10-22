-- Reuse physical authenticated Reply, then verify ordinary menu recovery.
_G.nsb7_sms_reply_continue=true
local source=debug.getinfo(1,'S').source:sub(2)
dofile(assert(source:match('^(.*[/\\])'))..'nsb7_pin_sms_reply.lua')
local machine=manager.machine
local fields={}
for _,port in pairs(machine.ioport.ports) do
    for _,field in pairs(port.fields) do fields[field.name]=field end
end
local recovery=coroutine.create(function()
    local delay=_G.nsb7_sms_recovery_delay or 0
    if delay>0 then
        for target=75,165,5 do
            assert(emu.wait(target-machine.time:as_double()))
            machine.screens[':screen']:snapshot('nsb7_sms_pending_'..target..'.png')
        end
    end
    for _,step in ipairs({{78,'End'},{82,'Left Softkey / Menu'},{85,'End'}}) do
        assert(emu.wait(step[1]+delay-machine.time:as_double()))
        local field=assert(fields[step[2]])
        machine:logerror(string.format('nsb7_sms_recovery: key=%s state=1 t=%.9f\n',step[2],machine.time:as_double()))
        field:set_value(1)
        assert(emu.wait(0.2))
        field:set_value(0)
        machine:logerror(string.format('nsb7_sms_recovery: key=%s state=0 t=%.9f\n',step[2],machine.time:as_double()))
        assert(emu.wait(0.8))
        machine.screens[':screen']:snapshot('nsb7_sms_recovery_'..(step[1]+delay)..'.png')
    end
    assert(emu.wait(88+delay-machine.time:as_double()))
    machine.screens[':screen']:snapshot('nsb7_sms_recovery_final.png')
    machine:logerror(string.format('nsb7_sms_recovery: done t=%.9f\n',machine.time:as_double()))
    machine:exit()
end)
_G.nsb7_sms_recovery=recovery
assert(coroutine.resume(recovery))
