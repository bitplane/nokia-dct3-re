-- Physical softkey presses after delivery; no card or firmware writes.
_G.nsb7_security_continue=true
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])')..'nsb7_page_observe.lua')
local machine=manager.machine
local fields={}
for _,port in pairs(machine.ioport.ports) do
    for _,field in pairs(port.fields) do fields[field.name]=field end
end
local input=coroutine.create(function()
    for _,target in ipairs(_G.nsb7_sms_read_times or {23,25}) do
        assert(emu.wait(target-machine.time:as_double()))
        local field=assert(fields['Left Softkey / Menu'])
        machine:logerror(string.format('nsb7_sms_input: key=Menu state=1 t=%.9f\n',machine.time:as_double()))
        field:set_value(1)
        assert(emu.wait(0.2))
        field:set_value(0)
        machine:logerror(string.format('nsb7_sms_input: key=Menu state=0 t=%.9f\n',machine.time:as_double()))
        assert(emu.wait(0.8))
        machine.screens[':screen']:snapshot('nsb7_sms_read_'..target..'.png')
    end
    if not _G.nsb7_sms_read_continue then
        assert(emu.wait(35-machine.time:as_double()))
        machine:exit()
    end
end)
_G.nsb7_sms_read_input=input
assert(coroutine.resume(input))
