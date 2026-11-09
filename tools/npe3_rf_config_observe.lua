-- Own-ROM configuration provenance, not RF readiness or physical bit decoding.
local machine=manager.machine
local cpu=assert(machine.devices[':dsp_c54x:cpu'])
local handles={}
local writes,selections=0,0
local entries=0
local counter_reads={}
local stops=0
handles[#handles+1]=cpu.spaces.io:install_read_tap(0x0d,0x0d,'npe3_stop_counter',function(_,value,mask)
    counter_reads[#counter_reads+1]={value=value,mask=mask,pc=cpu.state.GENPC.value,t=machine.time:as_double()}
    if #counter_reads>2 then table.remove(counter_reads,1) end
end)
handles[#handles+1]=cpu.spaces.io:install_write_tap(0x0b,0x0b,'npe3_capture_stop',function(_,value,mask)
    if cpu.state.GENPC.value~=0x1640 then return end
    stops=stops+1
    if stops>16384 then return end
    assert(#counter_reads==2,'6210 capture stop without two counter reads')
    machine:logerror(string.format('npe3_capture_stop: sequence=%d port=000b cpu_pc=001640 t=%.9f value=%04x tail=%04x length=%04x a=%010x mask=%04x observer_writes=0\n',
        stops,machine.time:as_double(),value,cpu.spaces.data:read_u16(0x3eb6),
        cpu.spaces.data:read_u16(0x036a),cpu.state.A.value&0xffffffffff,mask))
    local first,last=counter_reads[1],counter_reads[2]
    machine:logerror(string.format('rom6_capture_stop_poll: sequence=%d first=%04x first_pc=%06x first_t=%.9f first_mask=%04x last=%04x last_pc=%06x last_t=%.9f last_mask=%04x observer_writes=0\n',
        stops,first.value,first.pc,first.t,first.mask,last.value,last.pc,last.t,last.mask))
end)
handles[#handles+1]=cpu.spaces.data:install_read_tap(0x3ec8,0x3ec8,'npe3_rf_formatter',function(_,value)
        local owner=cpu.state.READPC.value
        if owner~=0x388e and owner~=0x389e then return end
        entries=entries+1
        if entries>8192 then return end
        machine:logerror(string.format('npe3_rf_formatter: sequence=%d entry=%04x t=%.9f a=%010x ar2=%04x flags=%04x class=%04x mode=%04x addend0=%04x addend1=%04x observer_writes=0\n',
            entries,owner,machine.time:as_double(),cpu.state.A.value,cpu.state.AR2.value,
            cpu.spaces.data:read_u16(0x3e7e),value,cpu.spaces.data:read_u16(0x51f),
            cpu.spaces.data:read_u16(0x3e7f),cpu.spaces.data:read_u16(0x3e80)))
    end)
-- End-of-formatter stores may target a command buffer rather than ports31/32.
handles[#handles+1]=cpu.spaces.data:install_write_tap(0x1000,0x26ff,'npe3_rf_formatter_store',function(address,value,mask)
    local owner=cpu.state.READPC.value
    if owner~=0x38c6 and owner~=0x38c7 then owner=cpu.state.EXECPC.value end
    if owner~=0x38c6 and owner~=0x38c7 then return end
    if entries==0 or entries>8192 then return end
    machine:logerror(string.format('npe3_rf_formatter_store: sequence=%d t=%.9f address=%04x value=%04x mask=%04x owner=%06x observer_writes=0\n',
        entries,machine.time:as_double(),address,value,mask,owner))
end)
handles[#handles+1]=cpu.spaces.data:install_write_tap(0x3f0f,0x3f10,'npe3_rf_config_source',function(address,value,mask)
    writes=writes+1
    if writes>128 then return end
    machine:logerror(string.format('npe3_rf_config_source: sequence=%d t=%.9f address=%04x value=%04x mask=%04x owner=%06x observer_writes=0\n',
        writes,machine.time:as_double(),address,value,mask,cpu.state.READPC.value))
    if address==0x3f10 and value~=0 then
        local file=assert(io.open('npe3_rf_config_prefix.be16','wb'))
        for word=0x3e7e,0x3f0f do file:write(string.pack('>I2',cpu.spaces.data:read_u16(word))) end
        file:write(string.pack('>I2',value));file:close()
    end
end)
handles[#handles+1]=cpu.spaces.data:install_write_tap(0x1275,0x1275,'npe3_rf_config_stage',function(_,value,mask)
    if value~=0x4a80 then return end
    selections=selections+1
    if selections>8192 then return end
    machine:logerror(string.format('npe3_rf_config_stage: sequence=%d t=%.9f value=%04x source0=%04x source1=%04x route=%04x mask=%04x owner=%06x observer_writes=0\n',
        selections,machine.time:as_double(),cpu.spaces.data:read_u16(0x1274),
        cpu.spaces.data:read_u16(0x3f0f),cpu.spaces.data:read_u16(0x3f10),
        cpu.spaces.data:read_u16(0x3efc),mask,cpu.state.READPC.value))
end)
handles[#handles+1]=emu.add_machine_stop_notifier(function()
    machine:logerror(string.format('npe3_capture_stop_complete: stops=%d cap=16384 observer_writes=0\n',stops))
    machine:logerror(string.format('npe3_rf_config_complete: writes=%d write_cap=128 stages=%d stage_cap=8192 observer_writes=0\n',writes,selections))
    machine:logerror(string.format('npe3_rf_formatter_complete: entries=%d cap=8192 observer_writes=0\n',entries))
end)
_G.npe3_rf_config_observer=handles
