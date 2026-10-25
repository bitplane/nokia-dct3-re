-- Shared own-matrix contact for clock fixtures, never a firmware event post.
return function(marker)
    local directory=debug.getinfo(1,'S').source:sub(2):match('^(.*[/\\])')
    local until_time=dofile(directory..'fixture_deadline.lua')
    local machine=manager.machine
    local memory=machine.devices[':maincpu'].spaces.program
    return function(name)
        for column=0,4 do
            for _,field in pairs(machine.ioport.ports[':COL.'..column].fields) do
                if field.name==name then
                    local row=0
                    while (1<<row)~=field.mask do row=row+1 end
                    local code=memory:read_u8(0x2e0b18+row*5+column)
                    for _,down in ipairs({1,0}) do
                        field:set_value(down)
                        machine:logerror(string.format('%s: name=%s key=%02x down=%d t=%.9f physical_only=1\n',marker,name,code,down,machine.time:as_double()))
                        if down==1 then until_time(machine.time:as_double()+.2,.2) end
                    end
                    return
                end
            end
        end
        error('missing physical key '..name)
    end
end
