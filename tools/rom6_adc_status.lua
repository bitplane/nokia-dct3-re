-- Passive P03 status-consumer census; no register or firmware writes.
local directory=assert(debug.getinfo(1,'S').source:sub(2):match('^(.*[/])'))
dofile(directory..'rom6_adc_burst.lua')
local machine=manager.machine
local cpu=assert(machine.devices[':dsp_c54x:cpu'])
local consumers={}
local reads,overflow=0,0
_G.rom6_status_read=cpu.spaces.io:install_read_tap(3,3,'rom6_status_read',function(_,value)
    reads=reads+1
    local pc=cpu.state.GENPC.value
    local item=consumers[pc]
    if not item then
        local count=0
        for _ in pairs(consumers) do count=count+1 end
        if count>=32 then overflow=overflow+1;return end
        item={count=0,clear=0,first=machine.time:as_double()}
        consumers[pc]=item
    end
    item.count=item.count+1
    if value&0x100==0 then item.clear=item.clear+1 end
    item.last=machine.time:as_double()
    item.value=value
end)
_G.rom6_status_stop=emu.add_machine_stop_notifier(function()
    local keys={}
    for pc in pairs(consumers) do keys[#keys+1]=pc end
    table.sort(keys)
    for _,pc in ipairs(keys) do
        local item=consumers[pc]
        machine:logerror(string.format('rom6_status_consumer: pc=%06x reads=%d bit8_clear=%d first=%.9f last=%.9f value=%04x observer_writes=0\n',
            pc,item.count,item.clear,item.first,item.last,item.value))
    end
    machine:logerror(string.format('rom6_status_complete: reads=%d consumers=%d overflow=%d cap=32 observer_writes=0\n',reads,#keys,overflow))
end)
