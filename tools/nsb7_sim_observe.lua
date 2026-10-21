-- Physical card transport observer; no APDU, message or guest-state injection.
local source=debug.getinfo(1,'S').source:sub(2)
_G.nsb7_observe_inserted_card=true
dofile(source:match('^(.*[/\\])')..'nsb7_input_observe.lua')
local machine=manager.machine
local cpu=machine.devices[':maincpu']
local memory=cpu.spaces.program
local taps={}
for _,kind in ipairs({'read','write'}) do
    local counts={}
    local function observe(address,value,mask)
        for lane=0,3 do
            local offset=(address&0xff)+lane
            local shift=(3-lane)*8
            if offset>=0x36 and offset<=0x3f and mask&(0xff<<shift)~=0 then
                local count=(counts[offset] or 0)+1
                counts[offset]=count
                local limit=offset==0x36 and 4096 or offset==0x37 and 8192 or 32
                if count>limit then goto next_lane end
                if kind=='write' and offset==0x36 then
                    machine:logerror(string.format('nsb7_sim_tx: index=%d value=%02x\n',count,(value>>shift)&0xff))
                end
                machine:logerror(string.format('nsb7_simi: kind=%s offset=%02x value=%02x pc=%08x t=%.9f observer_writes=0\n',
                    kind,offset,(value>>shift)&0xff,cpu.state['R15'].value,machine.time:as_double()))
            end
            ::next_lane::
        end
    end
    if kind=='read' then
        taps[#taps+1]=memory:install_read_tap(0x20034,0x2003f,'nsb7_simi_read',observe)
    else
        taps[#taps+1]=memory:install_write_tap(0x20034,0x2003f,'nsb7_simi_write',observe)
    end
end
_G.nsb7_sim_taps=taps
