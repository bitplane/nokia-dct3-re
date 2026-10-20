-- Read-only retained-upload observer, shared by explicit native fixtures.
return function(prefix)
    assert(prefix:match('^%w+$'),'simple log prefix required')
    local machine=manager.machine
    local dsp=machine.devices[':dsp_c54x:cpu']
    local writes,highest,words=0,0,{}
    _G[prefix..'_native_upload_tap']=dsp.spaces.program:install_write_tap(0x2800,0x2fff,prefix..'_native_upload_extent',function(offset,data,mask)
        assert(mask==0xffff,'partial program upload requires separate interpretation')
        writes=writes+1
        highest=math.max(highest,offset)
        words[offset]=data
    end)
    local captured=false
    emu.register_frame_done(function()
        if captured or machine.time:as_double()<1 then return end
        captured=true
        local unique,retained=0,0
        for address,word in pairs(words) do
            unique=unique+1
            local program=dsp.spaces.program:read_u16(address)
            local data=dsp.spaces.data:read_u16(address)
            if program==word and data==word then retained=retained+1 end
            if address<0x2808 then
                machine:logerror(string.format('%s_program_visible: address=%04x uploaded=%04x program=%04x data=%04x observer_writes=0\n',prefix,address,word,program,data))
            end
        end
        machine:logerror(string.format('%s_program_extent_summary: writes=%d unique=%d highest=%04x retained=%d observer_writes=0\n',prefix,writes,unique,highest,retained))
    end,prefix..' read-only native upload extent')
end
