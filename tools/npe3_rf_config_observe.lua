-- Own-ROM configuration provenance, not RF readiness or physical bit decoding.
local machine=manager.machine
local cpu=assert(machine.devices[':dsp_c54x:cpu'])
local handles={}
local writes,selections=0,0
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
    machine:logerror(string.format('npe3_rf_config_complete: writes=%d write_cap=128 stages=%d stage_cap=8192 observer_writes=0\n',writes,selections))
end)
_G.npe3_rf_config_observer=handles
