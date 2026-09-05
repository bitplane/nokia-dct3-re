local machine = manager.machine
local dsp = machine.devices[":dsp_c54x:cpu"]
local data = dsp.spaces["data"]
local checked = false

emu.register_periodic(function()
	if checked or machine.time.seconds < 3.0 then
		return
	end
	checked = true

	local producer = data:read_u16(0x0852)
	local consumer = data:read_u16(0x0853)
	local completion = data:read_u16(0x0880)
	local rx_producer = data:read_u16(0x08e4)
	local rx_consumer = data:read_u16(0x08e5)
	local measurement5 = data:read_u16(0x1f0c)
	local measurement6 = data:read_u16(0x1f0d)
	local security_verdict = data:read_u16(0x1f11)
	local idle = dsp.state["IDLE"].value
	local illegal = dsp.state["ILLEGAL"].value
	if completion ~= 0x1074 or producer ~= consumer or
			rx_producer ~= rx_consumer or measurement5 ~= 0x0016 or
			measurement6 ~= 0x0010 or idle == 0 or illegal ~= 0 then
		error(string.format(
			"ROM4 DSP gate failed: completion=%04x tx=%04x/%04x rx=%04x/%04x meas=%04x/%04x verdict=%04x idle=%d illegal=%d",
			completion, producer, consumer, rx_producer, rx_consumer,
			measurement5, measurement6, security_verdict, idle, illegal))
	end

	print(string.format(
		"ROM4 DSP coherent execution: PASS completion=1074 tx=%04x/%04x rx=%04x/%04x meas=0016/0010 verdict=%04x pc=%04x",
		producer, consumer, rx_producer, rx_consumer, security_verdict,
		dsp.state["PC"].value))
end)
