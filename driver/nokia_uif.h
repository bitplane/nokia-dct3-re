// license:BSD-3-Clause
// copyright-holders:Sandro Ronco, Gaz

#ifndef MAME_NOKIA_NOKIA_UIF_H
#define MAME_NOKIA_NOKIA_UIF_H

class nokia_uif_device : public device_t
{
public:
	nokia_uif_device(const machine_config &mconfig, const char *tag, device_t *owner, u32 clock = 0);

	static bool owns(offs_t offset);
	u8 read(offs_t offset) const;
	void write(offs_t offset, u8 data);

protected:
	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;

private:
	u8 m_regs[0x100] = { 0 };
};

DECLARE_DEVICE_TYPE(NOKIA_UIF, nokia_uif_device)

#endif // MAME_NOKIA_NOKIA_UIF_H
