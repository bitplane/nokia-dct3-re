-- Match the bounded original-helper Execute trace with actual operand bus reads.
local directory = assert(debug.getinfo(1,'S').source:match('^@(.*/)'), 'use an absolute autoboot script path')
dofile(directory .. 'mu4_raw_output_observe.lua')
local machine, handles, installed, closed = manager.machine, {}, false, false
local cpu = machine.devices[':cpu']
local selected = {[0x787e]=true,[0x787f]=true,[0x7880]=true,[0x7881]=true,
    [0xe87e]=true,[0xe87f]=true,[0xe880]=true,[0xe881]=true,
    [0xf000]=true,[0xf001]=true,[0xf0fe]=true,[0xf0ff]=true}
local count, upstream = 0, 0
local function record(address, value, mask)
    if machine:side_effects_disabled() then return end
    local t = machine.time:as_double()
    if address >= 0x7000 and ((t >= 1.0190183 and t <= 1.01901896) or
        (t >= 1.0210536 and t <= 1.02105428) or (t >= 1.0190675 and t <= 1.01906804) or
        (t >= 1.02110175 and t <= 1.02110228)) then
        upstream = upstream + 1
        if upstream <= 512 then
            machine:logerror(string.format('mu4_upstream_read: address=%04x value=%04x mask=%04x context_pc=%06x t=%.9f ownership=data_bus observer_writes=0\n',address,value,mask,cpu.state.GENPC.value,t))
        end
    end
    if not selected[address] then return end
    if not ((t >= 1.0191258 and t <= 1.01912643) or (t >= 1.0211590 and t <= 1.02115962)) then return end
    count = count + 1
    if count > 128 then return end
    machine:logerror(string.format('mu4_product_read: address=%04x value=%04x mask=%04x t=%.9f ownership=data_bus observer_writes=0\n',address,value,mask,t))
end
emu.register_frame_done(function()
    local t = machine.time:as_double()
    -- Delay the broad tap until after the upload; remove it after call 2.
    if not installed and t >= 1.0 then
        installed = true
        handles[1] = cpu.spaces.data:install_read_tap(0x60,0xffff,'product_reads',record)
    end
    if installed and not closed and t > 1.023 then
        handles[1]:remove()
        closed = true
    end
end)
local stop = emu.add_machine_stop_notifier(function()
    machine:logerror(string.format('mu4_product_terminal: reads=%u closed=%u capped=%u observer_writes=0 audio_acceptance=0\n',count,closed and 1 or 0,count > 128 and 1 or 0))
    machine:logerror(string.format('mu4_upstream_terminal: reads=%u closed=%u capped=%u observer_writes=0 audio_acceptance=0\n',upstream,closed and 1 or 0,upstream > 512 and 1 or 0))
end)
_G.mu4_product_handles = {handles,stop}
