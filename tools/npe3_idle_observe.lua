-- Bounded native CCCH metadata/scalar observations. No firmware writes.
local machine=manager.machine
assert(machine.system.name=='npe3ap5')
local cpu=assert(machine.devices[':dsp_c54x:cpu'])
local data=cpu.spaces.data
local handles={}
local formats,scalars,gates=0,0,0
local function word(address) return data:read_u16(address) end
handles[#handles+1]=data:install_write_tap(0x2f5d,0x2f5d,'npe3_idle_frame_format',function(_,value,mask)
    if machine.time:as_double()>24 then return end
    if word(0x2f1f)~=0x60 then return end
    local owner=cpu.state.EXECPC.value
    if owner~=0x4f27 and owner~=0x4f28 and owner~=0x4f2c then return end
    formats=formats+1
    if formats>128 then return end
    machine:logerror(string.format('npe3_idle_format: sequence=%d t=%.9f low=%04x mask=%04x owner=%06x selector=%04x primary=%04x,%04x secondary=%04x,%04x flags=%04x,%04x mode=%04x status=%04x receiver=%04x,%04x observer_writes=0\n',
        formats,machine.time:as_double(),value,mask,owner,word(0x2f1e),word(0x912),word(0x913),
        word(0x2456),word(0x2457),word(0x3eb5),word(0x586),word(0x250f),word(0x2f20),word(0x250e),word(0x2618)))
end)
handles[#handles+1]=data:install_write_tap(0x578,0x578,'npe3_idle_scalar',function(_,value,mask)
    if machine.time:as_double()>24 then return end
    scalars=scalars+1
    if scalars>64 then return end
    machine:logerror(string.format('npe3_idle_scalar: sequence=%d t=%.9f value=%04x mask=%04x owner=%08x read=%08x context=%06x option_flags=%04x previous=%04x observer_writes=0\n',
        scalars,machine.time:as_double(),value,mask,cpu.state.EXECPC.value,cpu.state.READPC.value,cpu.state.GENPC.value,word(0x3eb5),word(0x579)))
end)
handles[#handles+1]=data:install_write_tap(0x2f0a,0x2f0a,'npe3_idle_publish_gate',function(_,value,mask)
    -- This MVMD exposes the store's Read-slot address; EXECPC can be invalid.
    -- A generic bus-time PC is not evidence that the gate executed.
    if machine.time:as_double()>24 or
            (cpu.state.EXECPC.value~=0x5168 and cpu.state.READPC.value~=0x5168) or word(0x2f1f)~=0x60 then return end
    gates=gates+1
    if gates>256 then return end
    local packed={}
    for address=0x2f5b,0x2f6b do packed[#packed+1]=string.format('%04x',word(address)) end
    machine:logerror(string.format('npe3_idle_publish_gate: sequence=%d t=%.9f result=%04x value=%04x mask=%04x owner=%08x read=%08x context=%06x mode=%04x controls=%04x,%04x payload=%s observer_writes=0\n',
        gates,machine.time:as_double(),cpu.state.B.value&0xffff,value,mask,cpu.state.EXECPC.value,cpu.state.READPC.value,cpu.state.GENPC.value,
        word(0x250f),word(0x264e),word(0x51d),table.concat(packed):sub(1,66)))
end)
handles[#handles+1]=emu.add_machine_stop_notifier(function()
    machine:logerror(string.format('npe3_idle_observer_complete: formats=%d format_cap=128 scalars=%d scalar_cap=64 gates=%d gate_cap=256 window_end=24 observer_writes=0\n',formats,scalars,gates))
end)
_G.npe3_idle_observer=handles
