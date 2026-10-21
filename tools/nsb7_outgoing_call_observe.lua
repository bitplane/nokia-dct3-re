-- Dial through physical contacts; all call signalling remains firmware-owned.
_G.nsb7_security_continue=true
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])')..'nsb7_page_observe.lua')
local machine=manager.machine
local fields={}
for _,port in pairs(machine.ioport.ports) do
    for _,field in pairs(port.fields) do fields[field.name]=field end
end
local input=coroutine.create(function()
    for _,step in ipairs({{20,'Keypad 5'},{21,'Keypad 5'},{22,'Keypad 5'},
            {23,'Keypad 1'},{24,'Keypad 2'},{25,'Keypad 3'},{26,'Keypad 4'},
            {28,'Send'},{40,'End'}}) do
        assert(emu.wait(step[1]-machine.time:as_double()))
        local field=assert(fields[step[2]])
        machine:logerror(string.format('nsb7_outgoing_call_input: key=%s state=1 t=%.9f\n',step[2],machine.time:as_double()))
        field:set_value(1)
        assert(emu.wait(0.2))
        field:set_value(0)
        machine:logerror(string.format('nsb7_outgoing_call_input: key=%s state=0 t=%.9f\n',step[2],machine.time:as_double()))
        assert(emu.wait(0.8))
        machine.screens[':screen']:snapshot('nsb7_outgoing_call_'..step[1]..'.png')
    end
    assert(emu.wait(50-machine.time:as_double()))
    machine.screens[':screen']:snapshot('nsb7_outgoing_call_final.png')
    machine:logerror(string.format('nsb7_outgoing_call_done: t=%.9f\n',machine.time:as_double()))
    machine:exit()
end)
_G.nsb7_outgoing_call_input=input
assert(coroutine.resume(input))
