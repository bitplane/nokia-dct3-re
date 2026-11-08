-- Own-ROM reset-policy comparison; physical keys and observation only.
local directory=assert(debug.getinfo(1,'S').source:sub(2):match('^(.*[/])'))
local machine=manager.machine
local npe3=machine.system.name=='npe3r6' or machine.system.name=='npe3ref' or machine.system.name=='npe3afs' or machine.system.name=='npe3ad0' or machine.system.name=='npe3cp9' or machine.system.name=='npe3zp9' or machine.system.name=='npe3bp9' or machine.system.name=='npe3sp9' or machine.system.name=='npe3tp5'
assert(npe3 or machine.system.name=='nsm3r6')
dofile(directory..(npe3 and 'npe3_native_observe.lua' or 'rom6_native_ui.lua'))
local arm=assert(machine.devices[':maincpu'])
local entry=npe3 and 0x4ba486 or 0x2f604a
local selected=npe3 and 0x4ba4e0 or 0x2f60a4
local rearm=npe3 and 0x4ba532 or 0x2f60f6
local reason=npe3 and 0x4b0424 or 0x2f6d6c
local timer=npe3 and 0x3c19ba or 0x2879fe
local reset_timer=npe3 and 0x1c or 0x19
arm.debug:bpset(entry,'1',
    'logerror "rom6_reset_policy_entry: selector=%02x caller=%08x observer_writes=0\\n",r0,r14;g')
local handles={}
for _,site in ipairs({{selected,'selected'},{rearm,'rearm'}}) do
    local address,kind=site[1],site[2]
    handles[#handles+1]=arm.spaces.program:install_read_tap(address&~3,(address&~3)+3,'rom6_policy_'..kind,function()
        local pc=arm.state.GENPC.value
        if pc<address-8 or pc>address+8 then return end
        local sp=arm.state.R13.value
        -- Prefetch can visit an untaken tail: fields only, not Execute evidence.
        machine:logerror(string.format('rom6_reset_policy_fields: boundary=%s identity=Fetch_window t=%.9f reason=%02x selector=%02x nv=%02x status=%02x observer_writes=0\n',
            kind,machine.time:as_double(),arm.state.R4.value,arm.state.R5.value,
            arm.spaces.program:read_u8(sp),arm.spaces.program:read_u8(sp+1)))
    end)
end
_G.rom6_policy_observers=handles
if npe3 then
    local serial_writes=0
    handles[#handles+1]=arm.spaces.program:install_write_tap(0x20000,0xaffff,'npe3_policy_ccont_serial',function(address,value,mask)
        for lane=0,3 do
            local shift=24-8*lane
            if (address+lane)&0xff==0x2c and (mask>>shift)&0xff~=0 and serial_writes<256 then
                local byte=(value>>shift)&0xff
                if arm.state.GENPC.value~=0x4ec744 or (byte~=0x54 and byte~=0x56) then goto next_lane end
                serial_writes=serial_writes+1
                local stack={}
                for offset=0,9 do
                    stack[#stack+1]=string.format('%08x',arm.spaces.program:read_u32(arm.state.R13.value+4*offset))
                end
                machine:logerror(string.format('rom6_reset_policy_ccont_tx: ordinal=%d t=%.9f data=%02x pc=%08x selector=%02x mask=%02x stack=%s observer_writes=0\n',
                    serial_writes,machine.time:as_double(),byte,
                    arm.state.GENPC.value,arm.state.R6.value,arm.state.R7.value,table.concat(stack,',')))
            end
            ::next_lane::
        end
    end)
    handles[#handles+1]=arm.spaces.program:install_write_tap(0x20000,0xaffff,'npe3_policy_control',function(address,value,mask)
        if address&0x7ffff~=0x20000 or mask&0xff00==0 then return end
        machine:logerror(string.format('rom6_reset_policy_mad2: t=%.9f address=%08x value=%08x mask=%08x pc=%08x observer_writes=0\n',
            machine.time:as_double(),address,value,mask,arm.state.GENPC.value))
    end)
end
local dsp=assert(machine.devices[':dsp_c54x:cpu'])
local ccont_regs=emu.item(assert(machine.devices[':ccont'].items['0/m_regs']))
local backend=assert(machine.devices[':dsp_c54x'])
local frames=emu.item(assert(backend.items['0/m_frame_timer_expiries']))
_G.rom6_policy_samples=coroutine.create(function()
    for second=1,120 do
        if not emu.wait(1) then return end
        if second<=10 or second%10==0 then
            machine:logerror(string.format('rom6_reset_policy_state: t=%d ccont6=%02x frames=%d pc=%06x imr=%04x ifr=%04x st1=%04x producer=%04x consumer=%04x observer_writes=0\n',
                second,ccont_regs:read(6),frames:read(0),dsp.state.GENPC.value,
                dsp.state.IMR.value,dsp.state.IFR.value,dsp.state.ST1.value,
                dsp.spaces.data:read_u16(0x518),dsp.spaces.data:read_u16(0x519)))
        end
    end
end)
assert(coroutine.resume(_G.rom6_policy_samples))
for _,address in ipairs({0x28,0x58}) do
    handles[#handles+1]=dsp.spaces.data:install_write_tap(address,address,'rom6_policy_mmr',function(_,value)
        machine:logerror(string.format('rom6_reset_policy_mmr: t=%.9f address=%04x value=%04x pc=%06x observer_writes=0\n',
            machine.time:as_double(),address,value,dsp.state.GENPC.value))
    end)
end
local ring_writes=0
local previous_tail=0x80
handles[#handles+1]=dsp.spaces.data:install_write_tap(0x8e4,0x8e4,'rom6_policy_ring',function(_,value)
    ring_writes=ring_writes+1
    if ring_writes<=24 then
        local words={}
        local stack={}
        for offset=0,7 do
            stack[#stack+1]=string.format('%04x',dsp.spaces.data:read_u16((dsp.state.SP.value+offset)&0xffff))
        end
        local cursor=previous_tail
        while cursor~=value and cursor>=0x80 and cursor<=0xe3 and #words<100 do
            words[#words+1]=string.format('%04x',dsp.spaces.data:read_u16(0x800+cursor))
            cursor=cursor==0xe3 and 0x80 or cursor+1
        end
        machine:logerror(string.format('rom6_reset_policy_ring: t=%.9f tail=%04x pc=%06x words=%s stack=%s observer_writes=0\n',
            machine.time:as_double(),value,dsp.state.GENPC.value,table.concat(words,','),table.concat(stack,',')))
    end
    previous_tail=value
end)
handles[#handles+1]=emu.add_machine_stop_notifier(function()
    machine:logerror(string.format('rom6_reset_policy_ring_total: writes=%d observer_writes=0\n',ring_writes))
end)
local counter_reads=0
local counter_tap
counter_tap=dsp.spaces.io:install_read_tap(0x0d,0x0d,'rom6_policy_counter',function(_,value)
    local time=machine.time:as_double()
    if time<4 or counter_reads>=64 then return end
    counter_reads=counter_reads+1
    machine:logerror(string.format('rom6_reset_policy_counter: sample=%d t=%.12f value=%04x pc=%06x observer_writes=0\n',
        counter_reads,time,value,dsp.state.GENPC.value))
    if counter_reads==64 then counter_tap:remove() end
end)
handles[#handles+1]=counter_tap
arm.debug:bpset(reason,'1',
    'logerror "rom6_reset_policy_reason: value=%02x caller=%08x observer_writes=0\\n",r0,r14;g')
arm.debug:bpset(timer,string.format('r0==%x',reset_timer),
    'logerror "rom6_reset_policy_timer: id=%02x delay=%08x caller=%08x observer_writes=0\\n",r0,r1,r14;g')
if npe3 then
    arm.debug:bpset(0x305876,'1',
        'logerror "rom6_reset_policy_expired: reason=%02x observer_writes=0\\n",b@17fe48;g')
    arm.debug:bpset(0x30597e,'1',
        'logerror "rom6_reset_policy_idle_fault: value=%02x observer_writes=0\\n",r0;g')
end
