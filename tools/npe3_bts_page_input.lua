-- An external BTS action only: no MCU/DSP memory or message writes.
local machine=manager.machine
local when=tonumber(os.getenv('NPE3_NATIVE_PAGE_AT') or '')
if not when then return end
-- Paging needs the identity from a completed Location Updating: 6210 by 16 s, 5110 by 17 s.
assert((machine.system.name=='npe3ap5' and when>=16 and when<=20) or
       (machine.system.name=='nse1ap5' and when>=18 and when<=24))
_G.npe3_bts_page_input=coroutine.create(function()
    if not emu.wait(when) then return end
    local input=assert(machine.ioport.ports[':LAB_BTS'].fields['Laboratory BTS page'])
    input:set_value(1)
    if not emu.wait(0.05) then input:set_value(0);return end
    input:set_value(0)
end)
assert(coroutine.resume(_G.npe3_bts_page_input))
