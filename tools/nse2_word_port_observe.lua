-- Count unresolved peripheral traffic for the complete pre-reset window.
local machine=manager.machine
local cpu=assert(machine.devices[':dsp_c54x:cpu'])
local io=cpu.spaces.io
local reads,writes,last={},{},{}
_G.nse2_word_port_taps={}
for port=0x34,0x36 do
    reads[port],writes[port]=0,0
    _G.nse2_word_port_taps[#_G.nse2_word_port_taps+1]=io:install_read_tap(port,port,'nse2_word_port_read',function()
        if not _G.rom4_observation_read then reads[port]=reads[port]+1 end
    end)
    _G.nse2_word_port_taps[#_G.nse2_word_port_taps+1]=io:install_write_tap(port,port,'nse2_word_port_write',function(_,value,mask)
        writes[port]=writes[port]+1
        last[port]=value
        if writes[port]>16 then return end
        machine:logerror(string.format('nse2_word_port_write: port=%02x value=%04x mask=%04x pc=%04x n=%d t=%.9f observer_writes=0\n',
            port,value,mask,cpu.state['PC'].value,writes[port],machine.time:as_double()))
        if port==0x34 then
            machine:logerror(string.format('nse2_word_tx_state: state=%04x flags=%04x pending=%04x pointer=%04x t=%.9f observer_writes=0\n',
                cpu.spaces.data:read_u16(0x6c6),cpu.spaces.data:read_u16(0x6f3),
                cpu.spaces.data:read_u16(0x6c5),cpu.spaces.data:read_u16(0x6c2),machine.time:as_double()))
        end
    end)
end
local done=false
emu.register_frame_done(function()
    if done or machine.time:as_double()<30 then return end
    done=true
    for port=0x34,0x36 do
        machine:logerror(string.format('nse2_word_port_summary: port=%02x reads=%d writes=%d last=%04x t=%.9f observer_writes=0\n',
            port,reads[port],writes[port],last[port] or 0,machine.time:as_double()))
    end
    machine:logerror(string.format('nse2_word_terminal: state=%04x flags=%04x pending=%04x pointer=%04x t=%.9f observer_writes=0\n',
        cpu.spaces.data:read_u16(0x6c6),cpu.spaces.data:read_u16(0x6f3),
        cpu.spaces.data:read_u16(0x6c5),cpu.spaces.data:read_u16(0x6c2),machine.time:as_double()))
end,'NSE-2 unresolved word-port coverage')
