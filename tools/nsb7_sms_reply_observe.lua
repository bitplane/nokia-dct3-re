-- Enter Reply through the observed Options menu; physical input only.
_G.nsb7_sms_read_continue=true
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])')..'nsb7_sms_read_observe.lua')
local machine=manager.machine
local delay=_G.nsb7_sms_reply_delay or 0
local fields={}
for _,port in pairs(machine.ioport.ports) do
    for _,field in pairs(port.fields) do fields[field.name]=field end
end
local input=coroutine.create(function()
    for _,step in ipairs({{28,'Left Softkey / Menu'},{30,'Scroll Down'},
            {32,'Left Softkey / Menu'},{34,'Left Softkey / Menu'},{36,'Keypad 2'},
            {38,'Left Softkey / Menu'},{40,'Left Softkey / Menu'},
            {42,'Left Softkey / Menu'}}) do
        assert(emu.wait(step[1]+delay-machine.time:as_double()))
        local field=assert(fields[step[2]])
        machine:logerror(string.format('nsb7_sms_reply_input: key=%s state=1 t=%.9f\n',step[2],machine.time:as_double()))
        field:set_value(1)
        assert(emu.wait(0.2))
        field:set_value(0)
        machine:logerror(string.format('nsb7_sms_reply_input: key=%s state=0 t=%.9f\n',step[2],machine.time:as_double()))
        assert(emu.wait(0.8))
        machine.screens[':screen']:snapshot('nsb7_sms_reply_'..(step[1]+delay)..'.png')
    end
    for _,target in ipairs({44.3,45,46,49,70}) do
        assert(emu.wait(target+delay-machine.time:as_double()))
        machine.screens[':screen']:snapshot('nsb7_sms_reply_final_'..(target+delay)..'.png')
    end
    machine:logerror(string.format('nsb7_sms_reply_done: t=%.9f\n',machine.time:as_double()))
    machine:exit()
end)
_G.nsb7_sms_reply_input=input
assert(coroutine.resume(input))
