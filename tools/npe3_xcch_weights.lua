-- Passive common-block weighting census; no buffers or verdicts are supplied.
local machine=manager.machine
local cpu=assert(machine.devices[':dsp_c54x:cpu'])
-- State entries cached once: cpu.state[...] costs a container lookup per call.
local cpu_state=setmetatable({},{__index=function(t,k) local e=cpu.state[k]; rawset(t,k,e); return e end})
local writes=0
local tap=cpu.spaces.data:install_write_tap(0x2f00,0x2f03,'npe3_xcch_weights',function(address,value,mask)
    if cpu.spaces.data:read_u16(0x2f1f)~=0x60 then return end
    local owner=cpu_state.EXECPC.value
    if owner~=0x4e2b and owner~=0x4e3b then return end
    writes=writes+1
    if writes>4096 then return end
    local pointer=cpu.spaces.data:read_u16(0x264b)
    local base=pointer&0xfffc
    local raw={}
    for i=0,3 do raw[#raw+1]=string.format('%04x',cpu.spaces.data:read_u16(base+i)) end
    machine:logerror(string.format('npe3_xcch_weight: sequence=%d t=%.9f address=%04x value=%04x mask=%04x readpc=%06x execpc=%08x cpu_pc=%06x pointer=%04x raw=%s flags=%04x status=%04x ar0=%04x index=%04x bk=%04x ar3=%04x observer_writes=0\n',
        writes,machine.time:as_double(),address,value,mask,cpu_state.READPC.value,
        cpu_state.EXECPC.value,cpu_state.GENPC.value,pointer,table.concat(raw,','),
        cpu.spaces.data:read_u16(0x3eb5),cpu.spaces.data:read_u16(0x0586),cpu_state.AR0.value,
        cpu.spaces.data:read_u16(0x261f),cpu_state.BK.value,cpu_state.AR3.value))
end)
local stop=emu.add_machine_stop_notifier(function()
    machine:logerror(string.format('npe3_xcch_weight_complete: writes=%d cap=4096 observer_writes=0\n',writes))
end)
_G.npe3_xcch_weight_observers={tap,stop}
