-- MMIO conformance fixture, not evidence of organic firmware key handling.
local machine=manager.machine
assert(machine.system.name=='noki8290')
local space=machine.devices[':maincpu'].spaces.program
local done=false
local saved=nil
local power_phase=0
local function field(tag,mask)
    for _,f in pairs(machine.ioport.ports[':'..tag].fields) do
        if f.mask==mask then return f end
    end
    error('missing physical contact '..tag)
end
emu.register_frame_done(function()
    if power_phase~=0 then
        local actual=space:read_u8(0x2002a)&0x1f
        if power_phase==1 then
            assert(actual==0x0f,'Power column differs')
            field('PWR',1):set_value(0)
            power_phase=2
        else
            assert(actual==0x1f,'Power release differs')
            space:write_u8(0x200a8,saved.direction)
            space:write_u8(0x20028,saved.row)
            space:write_u8(0x2006b,saved.mask)
            machine:logerror('nsb7_keypad_conformance: PASS contacts=20 scans=100 power_mask=10 firmware_input_claim=0\n')
            power_phase=0
            machine:exit()
        end
        return
    end
    if done or machine.time:as_double()<0.5 then return end
    done=true
    saved={row=space:read_u8(0x20028),direction=space:read_u8(0x200a8),mask=space:read_u8(0x2006b)}
    space:write_u8(0x2006b,0x1f)
    space:read_u8(0x2002a)
    local count=0
    for row=1,4 do
        for column=0,4 do
            local key=field('COL.'..column,1<<row)
            key:set_value(1)
            for scan=0,4 do
                space:write_u8(0x200a8,1<<scan)
                space:write_u8(0x20028,(~(1<<scan))&0xff)
                local actual=space:read_u8(0x2002a)&0x1f
                local expected=scan==row and (0x1f&(~(1<<column))) or 0x1f
                assert(actual==expected,string.format('row=%d column=%d scan=%d got=%02x expected=%02x',row,column,scan,actual,expected))
            end
            key:set_value(0)
            count=count+1
        end
    end
    space:write_u8(0x200a8,0xe0)
    space:write_u8(0x20028,0x1f)
    local power=field('PWR',1)
    power:set_value(1)
    assert(count==20)
    -- Power is sampled through the board callback on the next input frame.
    power_phase=1
end,'NSB-7 matrix conformance')
