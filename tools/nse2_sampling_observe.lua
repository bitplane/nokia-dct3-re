-- Observe unresolved sampling controls without assigning event semantics.
local machine=manager.machine
local cpu=assert(machine.devices[':dsp_c54x:cpu'])
local io=assert(cpu.spaces.io)
local counts={[9]=0,[10]=0,[11]=0}
local last={}
_G.nse2_sampling_tap=io:install_write_tap(9,11,'nse2_sampling_controls',function(offset,value,mask)
    counts[offset]=counts[offset]+1
    last[offset]=value
    if counts[offset]<=12 then
        machine:logerror(string.format('nse2_sampling_control: port=%02x value=%04x mask=%04x n=%d pc=%04x mode=%04x t=%.9f observer_writes=0\n',
            offset,value,mask,counts[offset],cpu.state['PC'].value,
            cpu.spaces.data:read_u16(0xac),machine.time:as_double()))
    end
end)
local done=false
emu.register_frame_done(function()
    if done or machine.time:as_double()<30 then return end
    done=true
    for port=9,11 do
        machine:logerror(string.format('nse2_sampling_summary: port=%02x writes=%d last=%04x t=%.9f observer_writes=0\n',
            port,counts[port],last[port] or 0,machine.time:as_double()))
    end
end,'NSE-2 unresolved sampling control census')
