-- Observe actual DSP-side ring commits; never construct or deliver packets.
local machine=manager.machine
local cpu=assert(machine.devices[':dsp_c54x:cpu'])
local cap=machine.system.name=='npe3sp9' and 256 or 64
local commits=0
local tap=cpu.spaces.data:install_write_tap(0x8e4,0x8e4,'rom6_measure_rx_commit',function(_,value,mask)
    local previous=cpu.spaces.data:read_u16(0x8e4)
    if previous==value then return end
    commits=commits+1
    if commits>cap then return end
    local packed={}
    if previous>=0x80 and previous<0xe4 and value>=0x80 and value<0xe4 then
        local cursor=previous
        while cursor~=value do
            packed[#packed+1]=string.format('%04x',cpu.spaces.data:read_u16(0x800+cursor))
            cursor=cursor==0xe3 and 0x80 or cursor+1
        end
    end
    machine:logerror(string.format('rom6_measure_rx_commit: sequence=%d t=%.9f previous=%04x producer=%04x consumer=%04x words=%s mask=%04x cpu_pc=%06x observer_writes=0\n',
        commits,machine.time:as_double(),previous,value,cpu.spaces.data:read_u16(0x8e5),table.concat(packed,','),mask,cpu.state.GENPC.value))
end)
local stop=emu.add_machine_stop_notifier(function()
    machine:logerror(string.format('rom6_measure_rx_complete: commits=%d cap=%d observer_writes=0\n',commits,cap))
end)
_G.rom6_measure_ring_observer={tap,stop}
