-- Passive own-product FCCH residual and final capture controls.
local machine=manager.machine
local cpu=assert(machine.devices[':dsp_c54x:cpu'])
-- State entries cached once: cpu.state[...] costs a container lookup per call.
local cpu_state=setmetatable({},{__index=function(t,k) local e=cpu.state[k]; rawset(t,k,e); return e end})
local handles={}
-- Taps that can only return once their window has closed are removed a
-- millisecond later (a tap must not remove itself while it runs): the rest
-- of the run would otherwise call into Lua for every access.
local function retire(list)
    local co=coroutine.create(function()
        emu.wait(0.001)
        for _,handle in ipairs(list) do handle:remove() end
    end)
    assert(coroutine.resume(co))
end
local reads,peaks,states=0,0,0
local finished=false
local controls,control_count={},0
local counter,counter_time=0,0
local function word(address) return cpu.spaces.data:read_u16(address) end
handles[#handles+1]=cpu.spaces.io:install_read_tap(0x27,0x27,'npe3_fcch_reads',function()
    reads=reads+1
end)
handles[#handles+1]=cpu.spaces.io:install_read_tap(0x0d,0x0d,'npe3_fcch_counter',function(_,value)
    if finished then return end
    counter=value; counter_time=machine.time:as_double()
end)
handles[#handles+1]=cpu.spaces.data:install_write_tap(0x2f58,0x2f58,'npe3_fcch_peak',function(_,value)
    if finished then return end
    local owner=cpu_state.EXECPC.value
    if owner<0xb40 or owner>=0xe00 then return end
    peaks=peaks+1
    if peaks>128 then return end
    machine:logerror(string.format('rom6_burst_peak: sequence=%d t=%.9f index=%04x reads=%d mode=%04x producer=%04x consumer=%04x pc=%06x observer_writes=0\n',
        peaks,machine.time:as_double(),value,reads,word(0x56e),word(0x518),word(0x519),cpu_state.GENPC.value))
end)
for _,address in ipairs({0x2f65,0x2f6b,0x2f6c,0x2f7a,0x2f7b,0x2477,0x247a,0x247b,0x2627}) do
    handles[#handles+1]=cpu.spaces.data:install_write_tap(address,address,'npe3_fcch_state',function(at,value)
        if finished then return end
        local owner=cpu_state.EXECPC.value
        if owner<0xb40 or owner>=0xe00 then return end
        states=states+1
        if states>256 then return end
        machine:logerror(string.format('rom6_burst_fcch_state: sequence=%d t=%.9f address=%04x value=%04x pc=%06x reads=%d observer_writes=0\n',
            states,machine.time:as_double(),at,value,cpu_state.GENPC.value,reads))
        if at==0x2f6c then
            machine:logerror(string.format('rom6_burst_fcch_accumulate: t=%.9f previous=%04x value=%04x a=%010x b=%010x st1=%04x peak=%04x base=%04x index=%04x pc=%06x observer_writes=0\n',
                machine.time:as_double(),word(at),value,cpu_state.A.value,cpu_state.B.value,cpu_state.ST1.value,
                word(0x2f58),word(0x2f7b),word(0x2f79),cpu_state.GENPC.value))
        end
    end)
end
local function record(kind,address,value)
    if finished or reads==0 then return end
    control_count=control_count+1
    controls[(control_count-1)%64+1]={time=machine.time:as_double(),kind=kind,address=address,
        value=value,owner=cpu_state.EXECPC.value,counter=counter,counter_time=counter_time,
        frame=word(0x913),offset_high=word(0x247a),offset_low=word(0x247b),shift=word(0x2615)}
end
for _,port in ipairs({3,9,0xa,0xb,0xd,0xe,0xf}) do
    handles[#handles+1]=cpu.spaces.io:install_write_tap(port,port,'npe3_fcch_control',function(at,value)
        record('port',at,value)
    end)
end
for _,address in ipairs({0x913,0x91d,0x2f02}) do
    handles[#handles+1]=cpu.spaces.data:install_write_tap(address,address,'npe3_fcch_frame',function(at,value)
        record('data',at,value)
    end)
end
handles[#handles+1]=cpu.spaces.data:install_write_tap(0x10,0x10,'npe3_fcch_freeze',function(_,value)
    local owner=cpu_state.EXECPC.value
    if finished or value~=0x7737 or owner<0x76a7 or owner>0x76d9 then return end
    finished=true
    retire(handles)
    for ordinal=math.max(1,control_count-63),control_count do
        local row=controls[(ordinal-1)%64+1]
        machine:logerror(string.format('npe3_fcch_control: ordinal=%d t=%.9f kind=%s address=%04x value=%04x owner=%06x counter=%04x counter_t=%.9f frame=%04x offset=%04x,%04x shift=%04x observer_writes=0\n',
            ordinal,row.time,row.kind,row.address,row.value,row.owner,row.counter,row.counter_time,
            row.frame,row.offset_high,row.offset_low,row.shift))
    end
    machine:logerror(string.format('rom6_burst_peak_complete: writes=%d cap=128 observer_writes=0\n',peaks))
    machine:logerror(string.format('rom6_burst_fcch_complete: writes=%d cap=256 observer_writes=0\n',states))
end)
_G.npe3_fcch_schedule=handles
