-- Read-only queue activity census: matching snapshots alone do not prove traffic.
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsk3_native_radio_observe.lua')
local machine=manager.machine
local dsp=assert(machine.devices[':dsp_c54x:cpu'])
local counts,changes,last,taps={},{},{},{}
for _,address in ipairs({0x06b7,0x06b8}) do
    counts[address],changes[address]=0,0
    last[address]=dsp.spaces.data:read_u16(address)
    taps[#taps+1]=dsp.spaces.data:install_write_tap(address,address,'nsk3_rf_queue_'..address,function(_,value)
        counts[address]=counts[address]+1
        if value~=last[address] then changes[address]=changes[address]+1 end
        last[address]=value
        if counts[address]<=16 then
            machine:logerror(string.format('nsk3_rf_queue_write: address=%04x value=%04x pc=%04x t=%.9f observer_writes=0\n',
                address,value,dsp.state['PC'].value,machine.time:as_double()))
        end
    end)
end
_G.nsk3_rf_queue_taps=taps
local index=1
local times={5,20,40,45,65,90}
emu.register_frame_done(function()
    if not times[index] or machine.time:as_double()<times[index] then return end
    machine:logerror(string.format('nsk3_rf_queue_sample: t=%.6f producer_writes=%d consumer_writes=%d producer_changes=%d consumer_changes=%d producer=%04x consumer=%04x observer_writes=0\n',
        machine.time:as_double(),counts[0x06b7],counts[0x06b8],changes[0x06b7],changes[0x06b8],last[0x06b7],last[0x06b8]))
    index=index+1
end)
