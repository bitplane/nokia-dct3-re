-- Own MCU packet entry plus candidate shared-ROM4 scan state; observation only.
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsm1_pin_menu.lua')
local machine=manager.machine
local cpu=machine.devices[':maincpu']
local memory=cpu.spaces.program
local dsp=assert(machine.devices[':dsp_c54x:cpu'],'native compatibility DSP required')
local taps,counts={},{}
for _,address in ipairs({0x06b7,0x06b8,0x06d0,0x06d1}) do
    counts[address]=0
    taps[#taps+1]=dsp.spaces.data:install_write_tap(address,address,'nsm1_native_radio_'..address,function(_,value)
        counts[address]=counts[address]+1
        if counts[address]<=16 then
            machine:logerror(string.format('nsm1_native_radio_write: address=%04x value=%04x pc=%04x t=%.9f observer_writes=0\n',
                address,value,dsp.state['PC'].value,machine.time:as_double()))
        end
    end)
end
local packets=0
local types={}
taps[#taps+1]=memory:install_read_tap(0x2c1b94,0x2c1b97,'nsm1_native_radio_packet',function()
    if cpu.state['PC'].value~=0x2c1b96 then return end
    local pointer=cpu.state['R5'].value
    if pointer<0x100000 or pointer>0x11ff00 then return end
    packets=packets+1
    local kind=memory:read_u8(pointer+3)
    types[kind]=(types[kind] or 0)+1
    if packets<=64 then
        local bytes={}
        local length=math.min(memory:read_u8(pointer+2)+4,192)
        for offset=0,length-1 do bytes[#bytes+1]=string.format('%02x',memory:read_u8(pointer+offset)) end
        machine:logerror(string.format('nsm1_native_radio_packet: index=%d type=%02x pointer=%08x bytes=%s t=%.9f observer_writes=0\n',
            packets,kind,pointer,table.concat(bytes),machine.time:as_double()))
    end
end)
_G.nsm1_native_radio_taps=taps
local index=1
local times=_G.nsm1_native_radio_times or {5,20,40,45,65,90,100}
emu.register_frame_done(function()
    local now=machine.time:as_double()
    if not times[index] or now<times[index] then return end
    local data,program=dsp.spaces.data,dsp.spaces.program
    machine:logerror(string.format('nsm1_native_radio_sample: t=%.6f pending=%04x completed=%04x scan=%04x%04x qualified=%04x flags=%04x producer=%04x consumer=%04x threshold=%04x packets=%d queue_writes=%d/%d observer_writes=0\n',
        now,data:read_u16(0x06d0),data:read_u16(0x06d1),data:read_u16(0x1262),data:read_u16(0x1263),
        data:read_u16(0x1344),data:read_u16(0x1266),data:read_u16(0x06b7),data:read_u16(0x06b8),
        data:read_u16(0x219b),packets,counts[0x06b7],counts[0x06b8]))
    if times[index]==45 then
        local file=assert(io.open('nsm1_native_program_45s.bin','wb'))
        for address=0,0xffff do file:write(string.pack('>I2',program:read_u16(address))) end
        file:close()
    end
    for kind,count in pairs(types) do
        machine:logerror(string.format('nsm1_native_radio_type_count: t=%.6f type=%02x count=%d observer_writes=0\n',now,kind,count))
    end
    machine.screens[':screen']:snapshot('nsm1_native_radio_'..times[index]..'.png')
    index=index+1
end)
