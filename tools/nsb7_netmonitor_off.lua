-- Select Net monitor test 00 through physical contacts, not NV edits.
_G.nsb7_security_continue=true
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])')..'nsb7_security_observe.lua')
local machine=manager.machine
local fields={}
for _,port in pairs(machine.ioport.ports) do
    for _,field in pairs(port.fields) do fields[field.name]=field end
end
local input=coroutine.create(function()
    for _,step in ipairs({{26,'Left Softkey / Menu'},{29,'Scroll Up'},
            {32,'Left Softkey / Menu'},{35,'Keypad 0'},{36,'Keypad 0'},
            {38,'Left Softkey / Menu'}}) do
        assert(emu.wait(step[1]-machine.time:as_double()))
        local field=assert(fields[step[2]])
        machine:logerror(string.format('nsb7_netmonitor_input: key=%s state=1 t=%.9f\n',step[2],machine.time:as_double()))
        field:set_value(1)
        assert(emu.wait(0.2))
        field:set_value(0)
        machine:logerror(string.format('nsb7_netmonitor_input: key=%s state=0 t=%.9f\n',step[2],machine.time:as_double()))
        assert(emu.wait(0.8))
        machine.screens[':screen']:snapshot('nsb7_netmonitor_'..step[1]..'.png')
    end
    assert(emu.wait(46-machine.time:as_double()))
    machine.screens[':screen']:snapshot('nsb7_netmonitor_final.png')
    machine:logerror(string.format('nsb7_netmonitor_done: t=%.9f\n',machine.time:as_double()))
    machine:exit()
end)
_G.nsb7_netmonitor_input=input
assert(coroutine.resume(input))
