-- Read-only native service cadence; no completion or receive traffic supplied.
local machine=manager.machine
local cpu=assert(machine.devices[':dsp_c54x:cpu'])
-- C54x opcode reads expose PC one word ahead in the validated live run.
-- Entry reads are fetch observations, not proof of instruction retirement.
local retained,last={},{}
for _,address in ipairs({0x3924,0x3f4e,0x8dd0,0x9232}) do
    local reads,exact,plusone,other=0,0,0,0
    retained[#retained+1]=cpu.spaces.program:install_read_tap(address,address,'word_service_fetch',function()
        if _G.rom4_observation_read then return end
        local pc=cpu.state.PC.value
        reads=reads+1
        if pc==address then exact=exact+1 elseif pc==address+1 then plusone=plusone+1 else other=other+1 end
    end)
    retained[#retained+1]=emu.add_machine_stop_notifier(function()
        machine:logerror(string.format('nse2_word_service_fetch_summary: address=%04x reads=%d exact=%d plusone=%d other=%d observer_writes=0\n',address,reads,exact,plusone,other))
    end)
end
for _,address in ipairs({0x06b3,0x06c4}) do
    local count,minimum,maximum=0,nil,nil
    local increments,increment_last,increment_min,increment_max=0,nil,nil,nil
    local writers={}
    retained[#retained+1]=cpu.spaces.data:install_write_tap(address,address,'word_service_counter',function(_,value,mask)
        local now=machine.time:as_double()
        count=count+1
        local pc=cpu.state.PC.value
        writers[pc]=(writers[pc] or 0)+1
        if pc==(address==0x06b3 and 0x8e74 or 0x8e71) then
            increments=increments+1
            if increment_last then
                local delta=now-increment_last
                increment_min=math.min(increment_min or delta,delta)
                increment_max=math.max(increment_max or delta,delta)
            end
            increment_last=now
        end
        if last[address] then
            local delta=now-last[address]
            minimum=math.min(minimum or delta,delta)
            maximum=math.max(maximum or delta,delta)
        end
        last[address]=now
        if count<=4 then
            machine:logerror(string.format('nse2_word_service_counter: address=%04x value=%04x mask=%04x pc=%04x t=%.9f observer_writes=0\n',address,value,mask,cpu.state.PC.value,now))
        end
    end)
    retained[#retained+1]=emu.add_machine_stop_notifier(function()
        machine:logerror(string.format('nse2_word_service_counter_summary: address=%04x writes=%d min=%.9f max=%.9f observer_writes=0\n',address,count,minimum or 0,maximum or 0))
        machine:logerror(string.format('nse2_word_service_increment_summary: address=%04x increments=%d min=%.9f max=%.9f observer_writes=0\n',address,increments,increment_min or 0,increment_max or 0))
        local sites={}
        for pc in pairs(writers) do sites[#sites+1]=pc end
        table.sort(sites)
        for _,pc in ipairs(sites) do
            machine:logerror(string.format('nse2_word_service_writer: address=%04x pc=%04x writes=%d observer_writes=0\n',address,pc,writers[pc]))
        end
    end)
end
_G.nse2_word_service_observer=retained
