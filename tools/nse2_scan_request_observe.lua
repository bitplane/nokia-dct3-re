-- Attribute the scan extent to its actual host decoder, never change it.
local machine=manager.machine
local cpu=assert(machine.devices[':dsp_c54x:cpu'])
local data=cpu.spaces.data
_G.nse2_scan_request_taps={}
local mcu=machine.devices[':maincpu']
local memory=mcu.spaces.program
local setter_count=0
_G.nse2_bitmap_setter_tap=memory:install_read_tap(0x285170,0x285173,'nse2_bitmap_setter',function()
    if mcu.state['R15'].value~=0x285170 then return end
    setter_count=setter_count+1
    if setter_count>64 then return end
    local pointer=mcu.state['R0'].value
    local words={}
    for index=0,23 do words[#words+1]=string.format('%04x',memory:read_u16(pointer+index*2)) end
    machine:logerror(string.format('nse2_bitmap_setter: lr=%08x source=%08x bitmap=%s t=%.9f observer_writes=0\n',
        mcu.state['R14'].value,pointer,table.concat(words,','),machine.time:as_double()))
end)
local bitmap_writes=0
_G.nse2_bitmap_writer_tap=memory:install_write_tap(0x10a75c,0x10a78b,'nse2_bitmap_writer',function(offset,value,mask)
    bitmap_writes=bitmap_writes+1
    if bitmap_writes>128 then return end
    machine:logerror(string.format('nse2_bitmap_write: address=%08x value=%08x mask=%08x pc=%08x lr=%08x n=%d t=%.9f observer_writes=0\n',
        offset,value,mask,mcu.state['R15'].value,mcu.state['R14'].value,bitmap_writes,machine.time:as_double()))
end)
local bitmap_done=false
emu.register_frame_done(function()
    if bitmap_done or machine.time:as_double()<30 then return end
    bitmap_done=true
    machine:logerror(string.format('nse2_bitmap_write_summary: writes=%d setters=%d t=%.9f observer_writes=0\n',bitmap_writes,setter_count,machine.time:as_double()))
end,'NSE-2 channel bitmap writer coverage')
-- Use a positively observed branch target, not mid-line fetch absence.
for _,address in ipairs({0x20fd9c}) do
    local count=0
    _G.nse2_scan_request_taps[#_G.nse2_scan_request_taps+1]=memory:install_read_tap(address & ~3,(address & ~3)+3,'nse2_scan_bitmap',function()
        if mcu.state['R15'].value~=address then return end
        count=count+1
        if count>16 then return end
        local sp=mcu.state['R13'].value
        local bitmap={}
        for index=0,23 do bitmap[#bitmap+1]=string.format('%04x',memory:read_u16(sp+index*2)) end
        machine:logerror(string.format('nse2_scan_bitmap: address=%08x category=%08x count=%08x detail=%08x bitmap=%s t=%.9f observer_writes=0\n',
            address,memory:read_u32(sp+0x30),memory:read_u32(sp+0x34),memory:read_u32(sp+0x38),table.concat(bitmap,','),machine.time:as_double()))
    end)
end
_G.nse2_scan_request_taps[#_G.nse2_scan_request_taps+1]=memory:install_read_tap(0x29ad08,0x29ad0b,'nse2_scan_bitmap_copy',function()
    if mcu.state['R15'].value~=0x29ad08 or mcu.state['R14'].value~=0x28532f then return end
    local source=mcu.state['R1'].value
    local words={}
    for index=0,23 do words[#words+1]=string.format('%04x',memory:read_u16(source+index*2)) end
    machine:logerror(string.format('nse2_scan_bitmap_copy: source=%08x length=%d bitmap=%s t=%.9f observer_writes=0\n',
        source,mcu.state['R2'].value,table.concat(words,','),machine.time:as_double()))
end)
for _,address in ipairs({0x284,0x285}) do
    local count=0
    _G.nse2_scan_request_taps[#_G.nse2_scan_request_taps+1]=data:install_write_tap(address,address,'nse2_scan_extent',function(_,value,mask)
        count=count+1
        if count>16 then return end
        machine:logerror(string.format('nse2_scan_extent: address=%04x value=%04x mask=%04x pc=%04x n=%d t=%.9f observer_writes=0\n',
            address,value,mask,cpu.state['PC'].value,count,machine.time:as_double()))
    end)
end
for _,address in ipairs({0x3d5e,0x3d8f,0x3da3,0x3da5,0x3db1,0x92d}) do
    local count=0
    _G.nse2_scan_request_taps[#_G.nse2_scan_request_taps+1]=cpu.spaces.program:install_read_tap(address,address,'nse2_scan_selector',function()
        if _G.rom4_observation_read then return end
        local pc=cpu.state['PC'].value
        if pc~=address and pc~=address+1 then return end
        count=count+1
        if count>16 then return end
        local pointer=cpu.state['AR2'].value
        machine:logerror(string.format('nse2_scan_selector: address=%04x ar2=%04x input=%04x extent=%04x flags=%04x mode=%04x st0=%04x t=%.9f observer_writes=0\n',
            address,pointer,data:read_u16(pointer),data:read_u16(0x284),data:read_u16(0x6bc),
            data:read_u16(0x195d),cpu.state['ST0'].value,machine.time:as_double()))
    end)
end
