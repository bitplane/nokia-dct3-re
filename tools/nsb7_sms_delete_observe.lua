-- Select Erase from the observed first menu entry using physical keys.
_G.nsb7_sms_read_continue=true
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])')..'nsb7_sms_read_observe.lua')
local machine=manager.machine
local fields={}
for _,port in pairs(machine.ioport.ports) do
    for _,candidate in pairs(port.fields) do
        fields[candidate.name]=candidate
    end
end
local input=coroutine.create(function()
    for _,target in ipairs({28,30,32}) do
        assert(emu.wait(target-machine.time:as_double()))
        local cancel=target==32 and _G.nsb7_sms_delete_cancel
        local key=cancel and 'Back' or 'Menu'
        local field=fields[cancel and 'Right Softkey / C' or 'Left Softkey / Menu']
        machine:logerror(string.format('nsb7_sms_delete_input: key=%s state=1 t=%.9f\n',key,machine.time:as_double()))
        assert(field):set_value(1)
        assert(emu.wait(0.2))
        field:set_value(0)
        machine:logerror(string.format('nsb7_sms_delete_input: key=%s state=0 t=%.9f\n',key,machine.time:as_double()))
        assert(emu.wait(0.8))
        machine.screens[':screen']:snapshot('nsb7_sms_delete_'..target..'.png')
    end
    assert(emu.wait(38-machine.time:as_double()))
    machine.screens[':screen']:snapshot('nsb7_sms_delete_final.png')
    machine:exit()
end)
_G.nsb7_sms_delete_input=input
assert(coroutine.resume(input))
