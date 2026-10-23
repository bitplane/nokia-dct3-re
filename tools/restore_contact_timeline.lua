-- Restore only external fixture contacts; handset state comes from MAME.
return function(events,set_next,label)
    local machine=manager.machine
    return emu.add_machine_post_load_notifier(function()
        local now=machine.time:as_double()
        local levels,index={},1
        while index<=#events and events[index][1]<=now do
            local event=events[index]
            -- Separate Lua wrappers can refer to the same physical field.
            -- Callers provide a stable port/contact key in the fourth slot.
            levels[assert(event[4],'contact timeline requires stable identity')]={event[2],event[3]}
            index=index+1
        end
        set_next(index)
        for _,entry in pairs(levels) do entry[1]:set_value(entry[2]) end
        machine:logerror(label..': restored=1 physical_only=1\n')
    end)
end
