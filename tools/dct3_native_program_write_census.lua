-- Full address census: no sample source, program patch or control injection.
return function(prefix)
    local machine=manager.machine
    local dsp=assert(machine.devices[':dsp_c54x:cpu'])
    local pages={}
    local writes=0
    local high_writes=0
    local sampling=false
    _G[prefix..'_program_census_tap']=dsp.spaces.program:install_write_tap(0,0xffff,prefix..'_program_write_census',function(address,data,mask)
        writes=writes+1
        if address>=0x3000 then
            high_writes=high_writes+1
            if high_writes<=16 then machine:logerror(string.format('%s_program_high_write: address=%04x data=%04x mask=%04x pc=%04x pmst=%04x t=%.9f observer_writes=0\n',prefix,address,data,mask,dsp.state['PC'].value,dsp.state['PMST'].value,machine.time:as_double())) end
        end
        local page=address>>8
        local row=pages[page]
        if not row then row={writes=0,words={},minimum=address,maximum=address};pages[page]=row end
        row.writes=row.writes+1
        row.words[address]={data,mask}
        row.minimum=math.min(row.minimum,address)
        row.maximum=math.max(row.maximum,address)
    end)
    local identity_reads=0
    _G[prefix..'_program_identity_tap']=dsp.spaces.program:install_read_tap(0xff87,0xff87,prefix..'_program_identity_read',function(_,value)
        if sampling then return end
        identity_reads=identity_reads+1
        if identity_reads<=16 then machine:logerror(string.format('%s_program_identity_read: index=%d value=%04x pc=%04x t=%.9f observer_writes=0\n',prefix,identity_reads,value,dsp.state['PC'].value,machine.time:as_double())) end
    end)
    local captured=false
    emu.register_frame_done(function()
        if captured or machine.time:as_double()<1 then return end
        captured=true
        sampling=true
        local unique,above=0,0
        for page=0,255 do
            local row=pages[page]
            if row then
                local count,nonzero,visible=0,0,0
                for address,word in pairs(row.words) do
                    count=count+1
                    if word[1]~=0 then nonzero=nonzero+1 end
                    if word[2]==0xffff and dsp.spaces.program:read_u16(address)==word[1] then visible=visible+1 end
                end
                unique=unique+count
                if page>=0x30 then above=above+count end
                machine:logerror(string.format('%s_program_write_page: page=%02x writes=%d unique=%d min=%04x max=%04x nonzero=%d visible=%d observer_writes=0\n',prefix,page,row.writes,count,row.minimum,row.maximum,nonzero,visible))
            end
        end
        machine:logerror(string.format('%s_program_write_census: writes=%d unique=%d words_at_or_above_3000=%d t=%.9f observer_writes=0\n',prefix,writes,unique,above,machine.time:as_double()))
        sampling=false
    end,prefix..' full native program-write census')
end
