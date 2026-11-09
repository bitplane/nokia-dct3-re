-- Passive common-block weighting census; no buffers or verdicts are supplied.
local machine=manager.machine
local cpu=assert(machine.devices[':dsp_c54x:cpu'])
local writes=0
local tap=cpu.spaces.data:install_write_tap(0x2f00,0x2f03,'npe3_xcch_weights',function(address,value,mask)
    if cpu.spaces.data:read_u16(0x2f1f)~=0x60 then return end
    local owner=cpu.state.EXECPC.value
    if owner~=0x4e2b and owner~=0x4e3b then return end
    writes=writes+1
    if writes>4096 then return end
    local pointer=cpu.spaces.data:read_u16(0x264b)
    local base=pointer&0xfffc
    local raw={}
    for i=0,3 do raw[#raw+1]=string.format('%04x',cpu.spaces.data:read_u16(base+i)) end
    machine:logerror(string.format('npe3_xcch_weight: sequence=%d t=%.9f address=%04x value=%04x mask=%04x readpc=%06x execpc=%08x cpu_pc=%06x pointer=%04x raw=%s flags=%04x status=%04x ar0=%04x index=%04x bk=%04x ar3=%04x observer_writes=0\n',
        writes,machine.time:as_double(),address,value,mask,cpu.state.READPC.value,
        cpu.state.EXECPC.value,cpu.state.GENPC.value,pointer,table.concat(raw,','),
        cpu.spaces.data:read_u16(0x3eb5),cpu.spaces.data:read_u16(0x0586),cpu.state.AR0.value,
        cpu.spaces.data:read_u16(0x261f),cpu.state.BK.value,cpu.state.AR3.value))
end)
local stop=emu.add_machine_stop_notifier(function()
    machine:logerror(string.format('npe3_xcch_weight_complete: writes=%d cap=4096 observer_writes=0\n',writes))
end)
_G.npe3_xcch_weight_observers={tap,stop}
