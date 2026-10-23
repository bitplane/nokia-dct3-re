-- Observe unresolved sampling controls without assigning event semantics.
local machine=manager.machine
local cpu=assert(machine.devices[':dsp_c54x:cpu'])
local io=assert(cpu.spaces.io)
local counts={[9]=0,[10]=0,[11]=0}
local last={}
local handlers={0x363c,0x364e,0x369a,0x371d,0x3764,0x8e89,0x8ec5,0x8ed3,0xb7dc,0xb787}
local entries={}
_G.nse2_sampling_handler_taps={}
for _,address in ipairs(handlers) do
    entries[address]=0
    _G.nse2_sampling_handler_taps[#_G.nse2_sampling_handler_taps+1]=cpu.spaces.program:install_read_tap(address,address,'nse2_peripheral_handler',function()
        if _G.rom4_observation_read then return end
        local pc=cpu.state['PC'].value
        if pc==address or pc==address+1 then entries[address]=entries[address]+1 end
    end)
end
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
    for _,address in ipairs(handlers) do
        machine:logerror(string.format('nse2_peripheral_handler: address=%04x entries=%d t=%.9f observer_writes=0\n',
            address,entries[address],machine.time:as_double()))
    end
end,'NSE-2 unresolved sampling control census')
