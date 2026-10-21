-- Post-security menu exploration through physical contacts only.
_G.nsb7_security_continue=true
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])')..(_G.nsb7_menu_base_observer or 'nsb7_security_observe.lua'))
local machine=manager.machine
local fields={}
for _,port in pairs(machine.ioport.ports) do
    for _,field in pairs(port.fields) do fields[field.name]=field end
end
local input=coroutine.create(function()
    local steps=_G.nsb7_menu_steps or {{26,'Left Softkey / Menu'},{29,'Scroll Down'},{32,'Left Softkey / Menu'}}
    for _,step in ipairs(steps) do
        assert(emu.wait(step[1]-machine.time:as_double()))
        local field=assert(fields[step[2]])
        machine:logerror(string.format('nsb7_menu_input: key=%s state=1 t=%.9f\n',step[2],machine.time:as_double()))
        field:set_value(1)
        assert(emu.wait(0.2))
        field:set_value(0)
        machine:logerror(string.format('nsb7_menu_input: key=%s state=0 t=%.9f\n',step[2],machine.time:as_double()))
        assert(emu.wait(0.8))
        machine.screens[':screen']:snapshot('nsb7_menu_'..step[1]..'.png')
    end
    machine:exit()
end)
_G.nsb7_menu_input=input
assert(coroutine.resume(input))
