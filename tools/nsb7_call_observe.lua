-- Physical Answer/End only; signalling remains firmware-owned.
_G.nsb7_security_continue=true
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])')..'nsb7_page_observe.lua')
local machine=manager.machine
local fields={}
for _,port in pairs(machine.ioport.ports) do
    for _,field in pairs(port.fields) do fields[field.name]=field end
end
local times=_G.nsb7_call_times or {22,27}
local input=coroutine.create(function()
    -- Loading cancels old waits; save/load fixtures schedule their own cleanup.
    for _,step in ipairs({{times[1],'Send'},{times[2],'End'}}) do
        if not emu.wait(step[1]-machine.time:as_double()) then return end
        local field=assert(fields[step[2]])
        machine:logerror(string.format('nsb7_call_input: key=%s state=1 t=%.9f\n',step[2],machine.time:as_double()))
        field:set_value(1)
        if not emu.wait(0.2) then field:set_value(0); return end
        field:set_value(0)
        machine:logerror(string.format('nsb7_call_input: key=%s state=0 t=%.9f\n',step[2],machine.time:as_double()))
        if not emu.wait(0.8) then return end
        machine.screens[':screen']:snapshot('nsb7_call_'..step[2]..'.png')
    end
    if not emu.wait((_G.nsb7_call_finish_time or 35)-machine.time:as_double()) then return end
    machine.screens[':screen']:snapshot('nsb7_call_final.png')
    machine:exit()
end)
_G.nsb7_call_input=input
assert(coroutine.resume(input))
if _G.nsb7_call_ready_time then
    local captures=coroutine.create(function()
        assert(emu.wait(_G.nsb7_call_ready_time-machine.time:as_double()))
        machine.screens[':screen']:snapshot('nsb7_call_ready.png')
        machine:logerror(string.format('nsb7_call_ready: t=%.9f\n',machine.time:as_double()))
        assert(emu.wait((times[1]-1)-machine.time:as_double()))
        machine.screens[':screen']:snapshot('nsb7_call_ringing.png')
    end)
    _G.nsb7_call_host_captures=captures
    assert(coroutine.resume(captures))
end
