-- Passive native compatibility-DSP control census; no RF or state injection.
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsk3_liveness_observe.lua')
local machine=manager.machine
local dsp=assert(machine.devices[':dsp_c54x:cpu'],'native DSP required')
local taps,counts={},{}
for _,address in ipairs({0x06d0,0x06d1}) do
    counts[address]=0
    taps[#taps+1]=dsp.spaces.data:install_write_tap(address,address,'nsk3_native_control_'..address,function(_,value)
        counts[address]=counts[address]+1
        if counts[address]<=64 then
            machine:logerror(string.format('nsk3_native_control_write: address=%04x value=%04x pc=%04x t=%.9f observer_writes=0\n',
                address,value,dsp.state['PC'].value,machine.time:as_double()))
        end
    end)
end
_G.nsk3_native_control_taps=taps
local index=1
local times={5,20,40,45,65,90}
emu.register_frame_done(function()
    local now=machine.time:as_double()
    if not times[index] or now<times[index] then return end
    local data,program=dsp.spaces.data,dsp.spaces.program
    machine:logerror(string.format('nsk3_native_control_sample: t=%.6f pending=%04x completed=%04x control06e3=%04x state1949=%04x writes_pending=%d writes_completed=%d cap=64 observer_writes=0\n',
        now,data:read_u16(0x06d0),data:read_u16(0x06d1),data:read_u16(0x06e3),data:read_u16(0x1949),counts[0x06d0],counts[0x06d1]))
    machine:logerror(string.format('nsk3_native_scan_sample: t=%.6f counter=%04x%04x qualified=%04x flags=%04x producer=%04x consumer=%04x threshold=%04x observer_writes=0\n',
        now,data:read_u16(0x1262),data:read_u16(0x1263),data:read_u16(0x1344),data:read_u16(0x1266),
        data:read_u16(0x06b7),data:read_u16(0x06b8),data:read_u16(0x219b)))
    if times[index]==45 then
        local file=assert(io.open('nsk3_native_program_45s.bin','wb'))
        for address=0,0xffff do file:write(string.pack('>I2',program:read_u16(address))) end
        file:close()
        machine:logerror('nsk3_native_program_capture: words=65536 byteorder=big t=45 observer_writes=0\n')
    end
    if index==1 then
        for _,address in ipairs({0x3000,0x313f,0x47f6,0x4a49}) do
            local words={}
            for offset=0,7 do words[#words+1]=string.format('%04x',program:read_u16(address+offset)) end
            machine:logerror(string.format('nsk3_native_live_words: address=%04x words=%s observer_writes=0\n',address,table.concat(words)))
        end
    end
    index=index+1
end)
