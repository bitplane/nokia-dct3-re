-- Read-only last-window provenance. Queue items are inspected, never popped.
local machine=manager.machine
local cpu=assert(machine.devices[':dsp_c54x:cpu'])
local mfi=assert(machine.devices[':mfi_rx'])
local function item(name)
    local index=assert(mfi.items['0/'..name],name)
    return emu.item(index)
end
local head=item('m_state.head')
local consumed=item('m_state.consumed')
local ticks=item('m_state.source_ticks')
local values=item('m_state.words')
local raw,writers={},{}
local total,finished=0,false
local filter_captured=false
local function words(first,last)
    local result={}
    for address=first,last do
        result[#result+1]=string.format('%04x',cpu.spaces.data:read_u16(address))
    end
    return table.concat(result,',')
end
local function in_window()
    local t=machine.time:as_double()
    return not finished and t>=3 and t<=3.4
end
local handles={}
handles[#handles+1]=cpu.spaces.io:install_read_tap(0x27,0x27,'npe3_sch_raw_provenance',function(_,value)
    if not in_window() then return end
    -- Read taps run after the device handler: the consumed slot precedes head.
    local slot=(head:read(0)+95)%96
    assert(consumed:read(0)>0 and values:read(slot)==value,'queue read ownership differs')
    total=total+1
    raw[(total-1)%1024+1]={time=machine.time:as_double(),value=value,
        tick=ticks:read(slot),sequence=consumed:read(0),owner=cpu.state.READPC.value}
end)
handles[#handles+1]=cpu.spaces.data:install_write_tap(0xd8,0x197,'npe3_sch_sample_provenance',function(address,value)
    if not in_window() then return end
    writers[address]={value=value,time=machine.time:as_double(),owner=cpu.state.EXECPC.value,
        ar2=cpu.state.AR2.value,ar3=cpu.state.AR3.value}
end)
handles[#handles+1]=cpu.spaces.data:install_write_tap(0x10,0x10,'npe3_sch_provenance_commit',function(_,value)
    local owner=cpu.state.EXECPC.value
    if in_window() and not filter_captured and value==1 and owner>=0x7942 and owner<=0x7946 then
        filter_captured=true
        local source=cpu.state.AR2.value
        machine:logerror(string.format('npe3_sch_filter_input: t=%.9f source=%04x samples=%s coefficients=%s history=%s st1=%04x observer_writes=0\n',
            machine.time:as_double(),source,words(source,source+195),words(0x524,0x526),words(0x218,0x21b),cpu.state.ST1.value))
    end
    if finished or value~=0x7737 or owner<0x76a7 or owner>0x76d9 then return end
    finished=true
    for ordinal=math.max(1,total-1023),total do
        local row=raw[(ordinal-1)%1024+1]
        machine:logerror(string.format('npe3_sch_raw: ordinal=%d t=%.9f value=%04x source_tick=%d capture_sequence=%d owner=%06x observer_writes=0\n',
            ordinal,row.time,row.value,row.tick,row.sequence,row.owner))
    end
    local count=0
    for address=0xd8,0x197 do
        local row=writers[address]
        if row then
            count=count+1
            machine:logerror(string.format('npe3_sch_writer: address=%04x value=%04x t=%.9f owner=%06x ar2=%04x ar3=%04x observer_writes=0\n',
                address,row.value,row.time,row.owner,row.ar2,row.ar3))
        end
    end
    machine:logerror(string.format('npe3_sch_provenance: raw_total=%d raw_cap=1024 writers=%d begin=3 end=3.4 aperture_t=%.9f observer_writes=0\n',
        total,count,machine.time:as_double()))
end)
_G.npe3_sch_provenance=handles
