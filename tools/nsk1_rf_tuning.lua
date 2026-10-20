-- Passive own-firmware tuning provenance; no physical RF interpretation.
local source = debug.getinfo(1,'S').source:sub(2)
_G.nsk1_radio_no_call = true
dofile(source:match('^(.*[/\\])') .. 'nsk1_radio_observe.lua')
local machine = manager.machine
local dsp,main = machine.devices[':dsp_c54x:cpu'],machine.devices[':maincpu']
local taps,counts = {},{}
for _,space in ipairs({'program','data'}) do
    taps[#taps+1] = dsp.spaces[space]:install_write_tap(0x216a,0x216c,'nsk1_tuning_upload_'..space,function(address,value)
        local key = space..address
        counts[key] = (counts[key] or 0)+1
        if counts[key] > 8 then return end
        machine:logerror(string.format('nsk1_tuning_upload: space=%s address=%04x value=%04x pc=%04x t=%.9f observer_writes=0\n',space,address,value,dsp.state['PC'].value,machine.time:as_double()))
    end)
end
local reads = 0
taps[#taps+1] = main.spaces.program:install_read_tap(0x29821c,0x29822f,'nsk1_tuning_source',function(address,value,mask)
    reads = reads+1
    if reads > 32 then return end
    machine:logerror(string.format('nsk1_tuning_source: address=%08x value=%08x mask=%08x pc=%08x t=%.9f observer_writes=0\n',address,value,mask,main.state['R15'].value,machine.time:as_double()))
end)
for _,port in ipairs({0x31,0x32}) do
    taps[#taps+1] = dsp.spaces.io:install_write_tap(port,port,'nsk1_tuning_'..port,function(_,value)
        local data = dsp.spaces.data
        local channel = data:read_u16(0x1867)
        local key = port..':'..channel
        counts[key] = (counts[key] or 0)+1
        if machine.time:as_double() < 2 or counts[key] > 2 then return end
        machine:logerror(string.format('nsk1_tuning_pair: port=%02x value=%04x pc=%04x channel=%04x flags=%04x addend=%04x a=%010x t=%.9f observer_writes=0\n',port,value,dsp.state['PC'].value,channel,data:read_u16(0x06be),data:read_u16(0x216b),dsp.state['A'].value,machine.time:as_double()))
    end)
end
_G.nsk1_tuning_taps = taps
