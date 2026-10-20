-- Own NSK-3 accepted-record lifecycle: passive bus counts and one raw contact.
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsk3_startup_observe.lua')
local machine=manager.machine
local cpu=machine.devices[':maincpu']
local memory=cpu.spaces.program
local lcd,scan,rows,mask_writes=0,0,0,0
_G.nsk3_ui_lcd_tap=memory:install_write_tap(0x20028,0x2002f,'nsk3_ui_lcd',function(offset,value,mask)
    if (offset==0x20028 and mask & 0x000000ff ~=0) or
       (offset==0x2002c and mask & 0xff000000 ~=0) then lcd=lcd+1 end
end)
_G.nsk3_ui_scan_tap=memory:install_read_tap(0x20030,0x20033,'nsk3_ui_scan',function(offset,value,mask)
    if mask & 0xff000000 ~=0 then scan=scan+1 end
end)
_G.nsk3_ui_row_tap=memory:install_write_tap(0x20030,0x20033,'nsk3_ui_row',function(offset,value,mask)
    if mask & 0x00ff0000 ~=0 then rows=rows+1 end
    if mask & 0x000000ff ~=0 then
        mask_writes=mask_writes+1
        if mask_writes<=32 then machine:logerror(string.format('nsk3_ui_column_mask: value=%02x pc=%08x t=%.9f observer_writes=0\n',value&0xff,cpu.state['PC'].value,machine.time:as_double())) end
    end
end)
machine.debugger:command('temp3=0')
machine.debugger:command('bpset 23ad9c,temp3<4,{temp3=temp3+1;logerror "nsk3_ui_startup: flags=%02x pointer=%08x cycles=%016x observer_writes=0\\n",r0,r10,totalcycles;g}')
machine.debugger:command('temp9=0')
for _,site in ipairs({0x2aa870,0x2aa84e}) do
    machine.debugger:command(string.format('bpset %x,temp9<20,{temp9=temp9+1;logerror "nsk3_ui_key_route: site=%08x caller=%%08x cycles=%%016x observer_writes=0\\n",r14,totalcycles;g}',site,site))
end
local field
for _,candidate in pairs(machine.ioport.ports[':COL.1'].fields) do
    if candidate.mask==4 then field=candidate end
end
assert(field,'raw matrix contact unavailable')
local stage=0
local next_sample=1
emu.register_frame_done(function()
    local now=machine.time:as_double()
    if stage==0 and now>=8 then
        field:set_value(1);stage=1
        machine:logerror('nsk3_ui_contact: down=1 column=1 mask=04 firmware_writes=0\n')
    elseif stage==1 and now>=8.2 then
        field:set_value(0);stage=2
        machine:logerror('nsk3_ui_contact: down=0 column=1 mask=04 firmware_writes=0\n')
    end
    if now>=next_sample then
        machine:logerror(string.format('nsk3_ui_sample: t=%d lcd=%d scan=%d rows=%d pc=%08x irq=%02x irqmask=%02x row=%02x direction=%02x columnmask=%02x observer_writes=0\n',next_sample,lcd,scan,rows,cpu.state['PC'].value,memory:read_u8(0x20009),memory:read_u8(0x2000b),memory:read_u8(0x20031),memory:read_u8(0x2002f),memory:read_u8(0x20033)))
        next_sample=next_sample+1
    end
end)
