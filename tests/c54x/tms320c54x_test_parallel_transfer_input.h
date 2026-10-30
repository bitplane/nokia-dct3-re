// license:BSD-3-Clause
// copyright-holders:Gaz
#ifndef C54X_TEST_PARALLEL_TRANSFER_INPUT_H
#define C54X_TEST_PARALLEL_TRANSFER_INPUT_H
#include <array>
namespace c54x_test {
// Remaining mapped/peripheral/BSP descriptors; no arithmetic variant decoder.
struct parallel_transfer_fixture
{
	unsigned x, y, destination, snapshot, flags, source, operation;
	bool linear;
	u32 base;
	std::array<u16,4> initial, final;
	u16 xaddress, yaddress, xword, yword, st0, st1, pmst, result_st0, stored;
	u64 result;
};
}
#endif
