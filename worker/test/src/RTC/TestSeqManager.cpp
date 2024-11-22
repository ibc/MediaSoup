#include "common.hpp"
#include "RTC/SeqManager.hpp"
#include <catch2/catch_test_macros.hpp>
#include <string>
#include <vector>

using namespace RTC;

constexpr uint16_t MaxNumberFor15Bits = (1 << 15) - 1;

template<typename T>
struct TestSeqManagerInput
{
	TestSeqManagerInput(T input, T output, bool sync = false, bool drop = false, int64_t maxInput = -1)
	  : input(input), output(output), sync(sync), drop(drop), maxInput(maxInput)
	{
	}

	T input{ 0 };
	T output{ 0 };
	bool sync{ false };
	bool drop{ false };
	int64_t maxInput{ -1 };
};

template<typename T, uint8_t N>
void validate(SeqManager<T, N>& seqManager, std::vector<TestSeqManagerInput<T>>& inputs)
{
	for (auto& element : inputs)
	{
		if (element.sync)
		{
			seqManager.Sync(element.input - 1);
		}

		if (element.drop)
		{
			seqManager.Drop(element.input);
		}
		else
		{
			T output;

			seqManager.Input(element.input, output);

			// Covert to string because otherwise Catch will print uint8_t as char.
			REQUIRE(std::to_string(output) == std::to_string(element.output));

			if (element.maxInput != -1)
			{
				REQUIRE(std::to_string(element.maxInput) == std::to_string(seqManager.GetMaxInput()));
			}
		}
	}
}

SCENARIO("SeqManager", "[rtc][SeqMananger]")
{
	SECTION("0 is greater than 65000")
	{
		REQUIRE(SeqManager<uint16_t>::IsSeqHigherThan(0, 65000) == true);
	}

	SECTION("0 is greater than 32500 in range 15")
	{
		REQUIRE(SeqManager<uint16_t, 15>::IsSeqHigherThan(0, 32500) == true);
	}

	SECTION("receive ordered numbers, no sync, no drop")
	{
		// clang-format off
		std::vector<TestSeqManagerInput<uint16_t>> inputs =
		{
			{  0,  0, false, false },
			{  1,  1, false, false },
			{  2,  2, false, false },
			{  3,  3, false, false },
			{  4,  4, false, false },
			{  5,  5, false, false },
			{  6,  6, false, false },
			{  7,  7, false, false },
			{  8,  8, false, false },
			{  9,  9, false, false },
			{ 10, 10, false, false },
			{ 11, 11, false, false }
		};
		// clang-format on

		SeqManager<uint16_t> seqManager;
		SeqManager<uint16_t, 15> seqManager2;
		validate(seqManager, inputs);
		validate(seqManager2, inputs);
	}

	SECTION("receive ordered numbers, sync, no drop")
	{
		// clang-format off
		std::vector<TestSeqManagerInput<uint16_t>> inputs =
		{
			{  0, 0, false, false },
			{  1, 1, false, false },
			{  2, 2, false, false },
			{ 80, 3,  true, false },
			{ 81, 4, false, false },
			{ 82, 5, false, false },
			{ 83, 6, false, false },
			{ 84, 7, false, false }
		};
		// clang-format on

		SeqManager<uint16_t> seqManager;
		SeqManager<uint16_t, 15> seqManager2;
		validate(seqManager, inputs);
		validate(seqManager2, inputs);
	}

	SECTION("receive ordered numbers, sync, drop")
	{
		// clang-format off
		std::vector<TestSeqManagerInput<uint16_t>> inputs =
		{
			{  0,  0, false, false },
			{  1,  1, false, false },
			{  2,  2, false, false },
			{  3,  3, false, false },
			{  4,  4,  true, false }, // sync.
			{  5,  5, false, false },
			{  6,  6, false, false },
			{  7,  7,  true, false }, // sync.
			{  8,  0, false,  true }, // drop.
			{  9,  8, false, false },
			{ 11,  0, false,  true }, // drop.
			{ 10,  9, false, false },
			{ 12, 10, false, false },
		};
		// clang-format on

		SeqManager<uint16_t> seqManager;
		SeqManager<uint16_t, 15> seqManager2;
		validate(seqManager, inputs);
		validate(seqManager2, inputs);
	}

	SECTION("receive ordered wrapped numbers")
	{
		// clang-format off
		std::vector<TestSeqManagerInput<uint16_t>> inputs =
		{
			{ 65533, 65533, false, false },
			{ 65534, 65534, false, false },
			{ 65535, 65535, false, false },
			{     0,     0, false, false },
			{     1,     1, false, false }
		};
		// clang-format on

		SeqManager<uint16_t> seqManager;
		validate(seqManager, inputs);
	}

	SECTION("receive sequence numbers with a big jump")
	{
		// clang-format off
		std::vector<TestSeqManagerInput<uint16_t>> inputs =
		{
			{    0,    0, false, false },
			{    1,    1, false, false },
			{ 1000, 1000, false, false },
			{ 1001, 1001, false, false }
		};
		// clang-format on

		SeqManager<uint16_t> seqManager;
		SeqManager<uint16_t, 15> seqManager2;
		validate(seqManager, inputs);
		validate(seqManager2, inputs);
	}

	SECTION("receive out of order numbers with a big jump")
	{
		// clang-format off
		std::vector<TestSeqManagerInput<uint16_t>> inputs =
		{
			{      4,     4, false, false },
			{      3,     3, false, false },
			{  65535, 65535, false, false },
		};
		// clang-format on

		SeqManager<uint16_t> seqManager;
		validate(seqManager, inputs);
	}

	SECTION("receive mixed numbers with a big jump, drop before jump")
	{
		// clang-format off
		std::vector<TestSeqManagerInput<uint16_t>> inputs =
		{
			{   0,   0, false, false },
			{   1,   0, false,  true }, // drop.
			{ 100,  99, false, false },
			{ 100,  99, false, false },
			{ 103,   0, false,  true }, // drop.
			{ 101, 100, false, false }
		};
		// clang-format on

		SeqManager<uint16_t> seqManager;
		SeqManager<uint16_t, 15> seqManager2;
		validate(seqManager, inputs);
		validate(seqManager2, inputs);
	}

	SECTION("receive mixed numbers with a big jump, drop after jump")
	{
		// clang-format off
		std::vector<TestSeqManagerInput<uint16_t>> inputs =
		{
			{   0,   0, false, false },
			{   1,   1, false, false },
			{ 100,   0, false,  true }, // drop.
			{ 103,   0, false,  true }, // drop.
			{ 101, 100, false, false }
		};
		// clang-format on

		SeqManager<uint16_t> seqManager;
		SeqManager<uint16_t, 15> seqManager2;
		validate(seqManager, inputs);
		validate(seqManager2, inputs);
	}

	SECTION("drop, receive numbers newer and older than the one dropped")
	{
		// clang-format off
		std::vector<TestSeqManagerInput<uint16_t>> inputs =
		{
			{ 0, 0, false, false },
			{ 2, 0, false,  true }, // drop.
			{ 3, 2, false, false },
			{ 4, 3, false, false },
			{ 1, 1, false, false }
		};
		// clang-format on

		SeqManager<uint16_t> seqManager;
		SeqManager<uint16_t, 15> seqManager2;
		validate(seqManager, inputs);
		validate(seqManager2, inputs);
	}

	SECTION("receive mixed numbers, sync, drop")
	{
		// clang-format off
		std::vector<TestSeqManagerInput<uint16_t>> inputs =
		{
			{     0,  0, false, false },
			{     1,  1, false, false },
			{     2,  2, false, false },
			{     3,  3, false, false },
			{     7,  7, false, false },
			{     6,  0, false,  true }, // drop.
			{     8,  8, false, false },
			{    10, 10, false, false },
			{     9,  9, false, false },
			{    11, 11, false, false },
			{     0, 12,  true, false }, // sync.
			{     2, 14, false, false },
			{     3, 15, false, false },
			{     4, 16, false, false },
			{     5, 17, false, false },
			{     6, 18, false, false },
			{     7, 19, false, false },
			{     8, 20, false, false },
			{     9, 21, false, false },
			{    10, 22, false, false },
			{     9,  0, false,  true }, // drop.
			{    61, 23,  true, false }, // sync.
			{    62, 24, false, false },
			{    63, 25, false, false },
			{    64, 26, false, false },
			{    65, 27, false, false },
			{    11, 28,  true, false }, // sync.
			{    12, 29, false, false },
			{    13, 30, false, false },
			{    14, 31, false, false },
			{    15, 32, false, false },
			{     1, 33,  true, false }, // sync.
			{     2, 34, false, false },
			{     3, 35, false, false },
			{     4, 36, false, false },
			{     5, 37, false, false },
			{ 65533, 38,  true, false }, // sync.
			{ 65534, 39, false, false },
			{ 65535, 40, false, false },
			{     0, 41,  true, false }, // sync.
			{     1, 42, false, false },
			{     3,  0, false,  true }, // drop.
			{     4, 44, false, false },
			{     5, 45, false, false },
			{     6, 46, false, false },
			{     7, 47, false, false }
		};
		// clang-format on

		SeqManager<uint16_t> seqManager;
		validate(seqManager, inputs);
	}

	SECTION("receive ordered numbers, sync, no drop, increase input")
	{
		// clang-format off
		std::vector<TestSeqManagerInput<uint16_t>> inputs =
		{
			{  0,  0, false, false },
			{  1,  1, false, false },
			{  2,  2, false, false },
			{ 80,  3,  true, false },
			{ 81,  4, false, false },
			{ 82,  5, false, false },
			{ 83,  6, false, false },
			{ 84,  7, false, false }
		};
		// clang-format on

		SeqManager<uint16_t> seqManager;
		SeqManager<uint16_t, 15> seqManager2;
		validate(seqManager, inputs);
		validate(seqManager2, inputs);
	}

	SECTION("drop many inputs at the beginning (using uint16_t)")
	{
		// clang-format off
		std::vector<TestSeqManagerInput<uint16_t>> inputs =
		{
			{   1,   1, false, false },
			{   2,   0, false,  true }, // drop.
			{   3,   0, false,  true }, // drop.
			{   4,   0, false,  true }, // drop.
			{   5,   0, false,  true }, // drop.
			{   6,   0, false,  true }, // drop.
			{   7,   0, false,  true }, // drop.
			{   8,   0, false,  true }, // drop.
			{   9,   0, false,  true }, // drop.
			{ 120, 112, false, false },
			{ 121, 113, false, false },
			{ 122, 114, false, false },
			{ 123, 115, false, false },
			{ 124, 116, false, false },
			{ 125, 117, false, false },
			{ 126, 118, false, false },
			{ 127, 119, false, false },
			{ 128, 120, false, false },
			{ 129, 121, false, false },
			{ 130, 122, false, false },
			{ 131, 123, false, false },
			{ 132, 124, false, false },
			{ 133, 125, false, false },
			{ 134, 126, false, false },
			{ 135, 127, false, false },
			{ 136, 128, false, false },
			{ 137, 129, false, false },
			{ 138, 130, false, false },
			{ 139, 131, false, false }
		};
		// clang-format on

		SeqManager<uint16_t> seqManager;
		SeqManager<uint16_t, 15> seqManager2;
		validate(seqManager, inputs);
		validate(seqManager2, inputs);
	}

	SECTION("drop many inputs at the beginning (using uint8_t)")
	{
		// clang-format off
		std::vector<TestSeqManagerInput<uint8_t>> inputs =
		{
			{   1,   1, false, false },
			{   2,   0, false,  true }, // drop.
			{   3,   0, false,  true }, // drop.
			{   4,   0, false,  true }, // drop.
			{   5,   0, false,  true }, // drop.
			{   6,   0, false,  true }, // drop.
			{   7,   0, false,  true }, // drop.
			{   8,   0, false,  true }, // drop.
			{   9,   0, false,  true }, // drop.
			{ 120, 112, false, false },
			{ 121, 113, false, false },
			{ 122, 114, false, false },
			{ 123, 115, false, false },
			{ 124, 116, false, false },
			{ 125, 117, false, false },
			{ 126, 118, false, false },
			{ 127, 119, false, false },
			{ 128, 120, false, false },
			{ 129, 121, false, false },
			{ 130, 122, false, false },
			{ 131, 123, false, false },
			{ 132, 124, false, false },
			{ 133, 125, false, false },
			{ 134, 126, false, false },
			{ 135, 127, false, false },
			{ 136, 128, false, false },
			{ 137, 129, false, false },
			{ 138, 130, false, false },
			{ 139, 131, false, false }
		};
		// clang-format on

		SeqManager<uint8_t> seqManager;
		validate(seqManager, inputs);
	}

	SECTION("receive mixed numbers, sync, drop in range 15")
	{
		// clang-format off
		std::vector<TestSeqManagerInput<uint16_t>> inputs =
		{
			{     0,  0, false, false },
			{     1,  1, false, false },
			{     2,  2, false, false },
			{     3,  3, false, false },
			{     7,  7, false, false },
			{     6,  0, false,  true }, // drop.
			{     8,  8, false, false },
			{    10, 10, false, false },
			{     9,  9, false, false },
			{    11, 11, false, false },
			{     0, 12,  true, false }, // sync.
			{     2, 14, false, false },
			{     3, 15, false, false },
			{     4, 16, false, false },
			{     5, 17, false, false },
			{     6, 18, false, false },
			{     7, 19, false, false },
			{     8, 20, false, false },
			{     9, 21, false, false },
			{    10, 22, false, false },
			{     9,  0, false,  true }, // drop.
			{    61, 23,  true, false }, // sync.
			{    62, 24, false, false },
			{    63, 25, false, false },
			{    64, 26, false, false },
			{    65, 27, false, false },
			{    11, 28,  true, false }, // sync.
			{    12, 29, false, false },
			{    13, 30, false, false },
			{    14, 31, false, false },
			{    15, 32, false, false },
			{     1, 33,  true, false }, // sync.
			{     2, 34, false, false },
			{     3, 35, false, false },
			{     4, 36, false, false },
			{     5, 37, false, false },
			{ 32767, 38,  true, false }, // sync.
			{ 32768, 39, false, false },
			{ 32769, 40, false, false },
			{     0, 41,  true, false }, // sync.
			{     1, 42, false, false },
			{     3,  0, false,  true }, // drop.
			{     4, 44, false, false },
			{     5, 45, false, false },
			{     6, 46, false, false },
			{     7, 47, false, false }
		};
		// clang-format on

		SeqManager<uint16_t, 15> seqManager;
		validate(seqManager, inputs);
	}

	SECTION("drop many inputs at the beginning (using uint16_t with high values)")
	{
		// clang-format off
		std::vector<TestSeqManagerInput<uint16_t>> inputs =
		{
			{     1,     1, false, false },
			{     2,     0, false,  true }, // drop.
			{     3,     0, false,  true }, // drop.
			{     4,     0, false,  true }, // drop.
			{     5,     0, false,  true }, // drop.
			{     6,     0, false,  true }, // drop.
			{     7,     0, false,  true }, // drop.
			{     8,     0, false,  true }, // drop.
			{     9,     0, false,  true }, // drop.
			{ 32768, 32760, false, false },
			{ 32769, 32761, false, false },
			{ 32770, 32762, false, false },
			{ 32771, 32763, false, false },
			{ 32772, 32764, false, false },
			{ 32773, 32765, false, false },
			{ 32774, 32766, false, false },
			{ 32775, 32767, false, false },
			{ 32776, 32768, false, false },
			{ 32777, 32769, false, false },
			{ 32778, 32770, false, false },
			{ 32779, 32771, false, false },
			{ 32780, 32772, false, false }
		};
		// clang-format on

		SeqManager<uint16_t> seqManager;
		validate(seqManager, inputs);
	}

	SECTION("sync and drop some input near max-value")
	{
		// clang-format off
		std::vector<TestSeqManagerInput<uint16_t>> inputs =
		{
			{ 65530,  1,  true, false },
			{ 65531,  2, false, false },
			{ 65532,  3, false, false },
			{ 65533,  0, false, true  },
			{ 65534,  0, false, true  },
			{ 65535,  4, false, false },
			{     0,  5, false, false },
			{     1,  6, false, false },
			{     2,  7, false, false },
			{     3,  8, false, false }
		};
		// clang-format on

		SeqManager<uint16_t> seqManager;
		validate(seqManager, inputs);
	}

	SECTION("drop many inputs at the beginning (using uint16_t range 15 with high values)")
	{
		// clang-format off
		std::vector<TestSeqManagerInput<uint16_t>> inputs =
		{
			{     1,     1, false, false },
			{     2,     0, false,  true }, // drop.
			{     3,     0, false,  true }, // drop.
			{     4,     0, false,  true }, // drop.
			{     5,     0, false,  true }, // drop.
			{     6,     0, false,  true }, // drop.
			{     7,     0, false,  true }, // drop.
			{     8,     0, false,  true }, // drop.
			{     9,     0, false,  true }, // drop.
			{ 16384, 16376, false, false },
			{ 16385, 16377, false, false },
			{ 16386, 16378, false, false },
			{ 16387, 16379, false, false },
			{ 16388, 16380, false, false },
			{ 16389, 16381, false, false },
			{ 16390, 16382, false, false },
			{ 16391, 16383, false, false },
			{ 16392, 16384, false, false },
			{ 16393, 16385, false, false },
			{ 16394, 16386, false, false },
			{ 16395, 16387, false, false },
			{ 16396, 16388, false, false }
		};
		// clang-format on

		SeqManager<uint16_t, 15> seqManager;
		validate(seqManager, inputs);
	}

	SECTION("sync and drop some input near max-value in a 15bit range")
	{
		// clang-format off
		std::vector<TestSeqManagerInput<uint16_t>> inputs =
		{
			{ 32762,  1,  true, false, 32762 },
			{ 32763,  2, false, false, 32763 },
			{ 32764,  3, false, false, 32764 },
			{ 32765,  0, false, true,  32765 },
			{ 32766,  0, false, true,  32766 },
			{ 32767,  4, false, false, 32767 },
			{     0,  5, false, false,     0 },
			{     1,  6, false, false,     1 },
			{     2,  7, false, false,     2 },
			{     3,  8, false, false,     3 }
		};
		// clang-format on

		SeqManager<uint16_t, 15> seqManager;
		validate(seqManager, inputs);
	}

	SECTION("should update all values during multiple roll overs")
	{
		// clang-format off
		std::vector<TestSeqManagerInput<uint16_t>> inputs =
		{
			{ 0, 1, true, false, 0 },
		};
		for (uint16_t j = 0; j < 3; ++j) {
			for (uint16_t i = 1; i < std::numeric_limits<uint16_t>::max(); ++i) {
				const uint16_t output = i + 1;
				inputs.emplace_back( i, output, false, false, i );
			}
		}
		// clang-format on

		SeqManager<uint16_t> seqManager;
		validate(seqManager, inputs);
	}

	SECTION("should update all values during multiple roll overs (15 bits range)")
	{
		// clang-format off
		std::vector<TestSeqManagerInput<uint16_t>> inputs =
		{
			{ 0, 1, true, false, 0, },
		};
		for (uint16_t j = 0; j < 3; ++j) {
			for (uint16_t i = 1; i < MaxNumberFor15Bits; ++i) {
				const uint16_t output = i + 1;
				inputs.emplace_back( i, output, false, false, i );
			}
		}
		// clang-format on

		SeqManager<uint16_t, 15> seqManager;
		validate(seqManager, inputs);
	}

	SECTION("should produce same output for same old input before drop (15 bits range)")
	{
		// clang-format off
		std::vector<TestSeqManagerInput<uint16_t>> inputs =
		{
			{ 10,  1, true,  false }, // sync.
			{ 11,  2, false, false },
			{ 12,  3, false, false },
			{ 13,  4, false, false },
			{ 14,  0, false, true  }, // drop.
			{ 15,  5, false, false },
			{ 12,  3, false, false }
		};
		// clang-format on

		SeqManager<uint16_t, 15> seqManager;
		validate(seqManager, inputs);
	}

	SECTION("should properly clean previous cycle drops")
	{
		// clang-format off
		std::vector<TestSeqManagerInput<uint8_t>> inputs =
		{
			{ 1, 1, false, false },
			{ 2, 0, false, true  }, // Drop.
			{ 3, 2, false, false },
			{ 4, 3, false, false },
			{ 5, 4, false, false },
			{ 6, 5, false, false },
			{ 7, 6, false, false },
			{ 0, 7, false, false },
			{ 1, 0, false, false },
			{ 2, 1, false, false },
			{ 3, 2, false, false }
		};
		// clang-format on

		SeqManager<uint8_t, 3> seqManager;
		validate(seqManager, inputs);
	}

	SECTION("dropped inputs to be removed going out of range, 1.")
	{
		// clang-format off
		std::vector<TestSeqManagerInput<uint16_t>> inputs =
		{
			{ 36964, 36964, false, false },
			{ 25923,     0, false, true  }, // Drop.
			{ 25701, 25701, false, false },
			{ 17170,     0, false, true  }, // Drop.
			{ 25923, 25923, false, false },
			{  4728,     0, false, true  }, // Drop.
			{ 17170, 17170, false, false },
			{ 30738,     0, false, true  }, // Drop.
			{  4728,  4728, false, false },
			{  4806,     0, false, true  }, // Drop.
			{ 30738, 30738, false, false },
			{ 50886,     0, false, true  }, // Drop.
			{  4806,  4805, false, false }, // Previously dropped.
			{ 50774,     0, false, true  }, // Drop.
			{ 50886,  4805, false, false }, // Previously dropped.
			{ 22136,     0, false, true  }, // Drop.
			{ 50774, 50773, false, false },
			{ 30910,     0, false, true  }, // Drop.
			{ 22136, 50773, false, false }, // Previously dropped.
			{ 48862,     0, false, true  }, // Drop.
			{ 30910, 30909, false, false },
			{ 56832,     0, false, true  }, // Drop.
			{ 48862, 48861, false, false },
			{     2,     0, false, true  }, // Drop.
			{ 56832, 48861, false, false }, // Previously dropped.
			{   530,     0, false, true  }, // Drop.
			{     2, 48861, false, false }, // Previously dropped.
		};
		// clang-format on

		SeqManager<uint16_t> seqManager;
		validate(seqManager, inputs);
	}

	SECTION("dropped inputs to be removed go out of range, 2.")
	{
		// clang-format off
		std::vector<TestSeqManagerInput<uint16_t>> inputs =
		{
			{ 36960, 36960, false, false },
			{  3328,     0, false, true  }, // Drop.
			{ 24589, 24588, false, false },
			{   120,     0, false, true  }, // Drop.
			{  3328, 24588, false, false }, // Previously dropped.
			{ 30848,     0, false, true  }, // Drop.
			{   120,   120, false, false },
		};
		// clang-format on

		SeqManager<uint16_t> seqManager;
		validate(seqManager, inputs);
	}

	SECTION("dropped inputs to be removed go out of range, 3.")
	{
		// clang-format off
		std::vector<TestSeqManagerInput<uint16_t>> inputs =
		{
			{ 36964, 36964, false, false },
			{ 65396 ,    0, false, true  }, // Drop.
			{ 25855, 25854, false, false },
			{ 29793 ,    0, false, true  }, // Drop.
			{ 65396, 25854, false, false }, // Previously dropped.
			{ 25087,    0,  false, true  }, // Drop.
			{ 29793, 25854, false, false }, // Previously dropped.
			{ 65535 ,    0, false, true  }, // Drop.
			{ 25087, 25086, false, false },
		};
		// clang-format on

		SeqManager<uint16_t> seqManager;
		validate(seqManager, inputs);
	}

	SECTION("receive ordered numbers, no sync, no drop (with initial output)")
	{
		// clang-format off
		std::vector<TestSeqManagerInput<uint16_t>> inputs =
		{
			{  0, 1000, false, false },
			{  1, 1001, false, false },
			{  2, 1002, false, false },
			{  3, 1003, false, false },
			{  4, 1004, false, false },
			{  5, 1005, false, false },
			{  6, 1006, false, false },
			{  7, 1007, false, false },
			{  8, 1008, false, false },
			{  9, 1009, false, false },
			{ 10, 1010, false, false },
			{ 11, 1011, false, false }
		};
		// clang-format on

		SeqManager<uint16_t> seqManager(/*initialOutput*/ 1000u);
		SeqManager<uint16_t, 15> seqManager2(/*initialOutput*/ 1000u);
		validate(seqManager, inputs);
		validate(seqManager2, inputs);
	}

	SECTION("receive ordered numbers, sync, no drop (with initial output)")
	{
		// clang-format off
		std::vector<TestSeqManagerInput<uint16_t>> inputs =
		{
			{  0, 2000, false, false },
			{  1, 2001, false, false },
			{  2, 2002, false, false },
			{ 80, 2003,  true, false },
			{ 81, 2004, false, false },
			{ 82, 2005, false, false },
			{ 83, 2006, false, false },
			{ 84, 2007, false, false }
		};
		// clang-format on

		SeqManager<uint16_t> seqManager(/*initialOutput*/ 2000u);
		SeqManager<uint16_t, 15> seqManager2(/*initialOutput*/ 2000u);
		validate(seqManager, inputs);
		validate(seqManager2, inputs);
	}

	SECTION("receive ordered numbers, sync, drop (with initial output)")
	{
		// clang-format off
		std::vector<TestSeqManagerInput<uint16_t>> inputs =
		{
			{  0, 3000, false, false },
			{  1, 3001, false, false },
			{  2, 3002, false, false },
			{  3, 3003, false, false },
			{  4, 3004,  true, false }, // sync.
			{  5, 3005, false, false },
			{  6, 3006, false, false },
			{  7, 3007,  true, false }, // sync.
			{  8, 3000, false,  true }, // drop.
			{  9, 3008, false, false },
			{ 11, 3000, false,  true }, // drop.
			{ 10, 3009, false, false },
			{ 12, 3010, false, false },
		};
		// clang-format on

		SeqManager<uint16_t> seqManager(/*initialOutput*/ 3000u);
		SeqManager<uint16_t, 15> seqManager2(/*initialOutput*/ 3000u);
		validate(seqManager, inputs);
		validate(seqManager2, inputs);
	}

	SECTION("receive ordered wrapped numbers (with initial output)")
	{
		// clang-format off
		std::vector<TestSeqManagerInput<uint16_t>> inputs =
		{
			{ 65533,  997, false, false },
			{ 65534,  998, false, false },
			{ 65535,  999, false, false },
			{     0, 1000, false, false },
			{     1, 1001, false, false }
		};
		// clang-format on

		SeqManager<uint16_t> seqManager(/*initialOutput*/ 1000u);
		validate(seqManager, inputs);
	}

	SECTION("receive sequence numbers with a big jump (with initial output)")
	{
		// clang-format off
		std::vector<TestSeqManagerInput<uint16_t>> inputs1 =
		{
			{    0, 32000, false, false },
			{    1, 32001, false, false },
			{ 1000, 33000, false, false },
			{ 1001, 33001, false, false }
		};
		// clang-format on

		SeqManager<uint16_t> seqManager(/*initialOutput*/ 32000u);
		validate(seqManager, inputs1);

		// clang-format off
		std::vector<TestSeqManagerInput<uint16_t>> inputs2 =
		{
			{    0, 32000, false, false },
			{    1, 32001, false, false },
			{ 1000,   232, false, false },
			{ 1001,   233, false, false }
		};
		// clang-format on

		SeqManager<uint16_t, 15> seqManager2(/*initialOutput*/ 32000u);
		validate(seqManager2, inputs2);
	}

	SECTION("receive out of order numbers with a big jump (with initial output)")
	{
		// clang-format off
		std::vector<TestSeqManagerInput<uint16_t>> inputs =
		{
			{      4, 1004, false, false },
			{      3, 1003, false, false },
			{  65535,  999, false, false },
		};
		// clang-format on

		SeqManager<uint16_t> seqManager(/*initialOutput*/ 1000u);
		validate(seqManager, inputs);
	}

	SECTION("receive mixed numbers with a big jump, drop before jump (with initial output)")
	{
		// clang-format off
		std::vector<TestSeqManagerInput<uint16_t>> inputs =
		{
			{   0, 1000, false, false },
			{   1, 1000, false,  true }, // drop.
			{ 100, 1099, false, false },
			{ 100, 1099, false, false },
			{ 103, 1000, false,  true }, // drop.
			{ 101, 1100, false, false }
		};
		// clang-format on

		SeqManager<uint16_t> seqManager(/*initialOutput*/ 1000);
		SeqManager<uint16_t, 15> seqManager2(/*initialOutput*/ 1000);
		validate(seqManager, inputs);
		validate(seqManager2, inputs);
	}

	SECTION("receive mixed numbers with a big jump, drop after jump (with initial output)")
	{
		// clang-format off
		std::vector<TestSeqManagerInput<uint16_t>> inputs =
		{
			{   0, 2000, false, false },
			{   1, 2001, false, false },
			{ 100, 2000, false,  true }, // drop.
			{ 103, 2000, false,  true }, // drop.
			{ 101, 2100, false, false }
		};
		// clang-format on

		SeqManager<uint16_t> seqManager(/*initialOutput*/ 2000);
		SeqManager<uint16_t, 15> seqManager2(/*initialOutput*/ 2000);
		validate(seqManager, inputs);
		validate(seqManager2, inputs);
	}

	SECTION("drop, receive numbers newer and older than the one dropped (with initial output)")
	{
		// clang-format off
		std::vector<TestSeqManagerInput<uint16_t>> inputs =
		{
			{ 0, 2000, false, false },
			{ 2, 2000, false,  true }, // drop.
			{ 3, 2002, false, false },
			{ 4, 2003, false, false },
			{ 1, 2001, false, false }
		};
		// clang-format on

		SeqManager<uint16_t> seqManager(/*initialOutput*/ 2000);
		SeqManager<uint16_t, 15> seqManager2(/*initialOutput*/ 2000);
		validate(seqManager, inputs);
		validate(seqManager2, inputs);
	}

	SECTION("receive mixed numbers, sync, drop (with initial output)")
	{
		// clang-format off
		std::vector<TestSeqManagerInput<uint16_t>> inputs =
		{
			{     0, 10000, false, false },
			{     1, 10001, false, false },
			{     2, 10002, false, false },
			{     3, 10003, false, false },
			{     7, 10007, false, false },
			{     6, 10000, false,  true }, // drop.
			{     8, 10008, false, false },
			{    10, 10010, false, false },
			{     9, 10009, false, false },
			{    11, 10011, false, false },
			{     0, 10012,  true, false }, // sync.
			{     2, 10014, false, false },
			{     3, 10015, false, false },
			{     4, 10016, false, false },
			{     5, 10017, false, false },
			{     6, 10018, false, false },
			{     7, 10019, false, false },
			{     8, 10020, false, false },
			{     9, 10021, false, false },
			{    10, 10022, false, false },
			{     9, 10000, false,  true }, // drop.
			{    61, 10023,  true, false }, // sync.
			{    62, 10024, false, false },
			{    63, 10025, false, false },
			{    64, 10026, false, false },
			{    65, 10027, false, false },
			{    11, 10028,  true, false }, // sync.
			{    12, 10029, false, false },
			{    13, 10030, false, false },
			{    14, 10031, false, false },
			{    15, 10032, false, false },
			{     1, 10033,  true, false }, // sync.
			{     2, 10034, false, false },
			{     3, 10035, false, false },
			{     4, 10036, false, false },
			{     5, 10037, false, false },
			{ 65533, 10038,  true, false }, // sync.
			{ 65534, 10039, false, false },
			{ 65535, 10040, false, false },
			{     0, 10041,  true, false }, // sync.
			{     1, 10042, false, false },
			{     3, 10000, false,  true }, // drop.
			{     4, 10044, false, false },
			{     5, 10045, false, false },
			{     6, 10046, false, false },
			{     7, 10047, false, false }
		};
		// clang-format on

		SeqManager<uint16_t> seqManager(/*initialOutput*/ 10000);
		validate(seqManager, inputs);
	}

	SECTION("receive ordered numbers, sync, no drop, increase input (with initial output)")
	{
		// clang-format off
		std::vector<TestSeqManagerInput<uint16_t>> inputs =
		{
			{  0,  1, false, false },
			{  1,  2, false, false },
			{  2,  3, false, false },
			{ 80,  4,  true, false },
			{ 81,  5, false, false },
			{ 82,  6, false, false },
			{ 83,  7, false, false },
			{ 84,  8, false, false }
		};
		// clang-format on

		SeqManager<uint16_t> seqManager(/*initialOutput*/ 1);
		SeqManager<uint16_t, 15> seqManager2(/*initialOutput*/ 1);
		validate(seqManager, inputs);
		validate(seqManager2, inputs);
	}

	SECTION("drop many inputs at the beginning (using uint16_t) (with initial output)")
	{
		// clang-format off
		std::vector<TestSeqManagerInput<uint16_t>> inputs =
		{
			{   1, 1001, false, false },
			{   2, 1000, false,  true }, // drop.
			{   3, 1000, false,  true }, // drop.
			{   4, 1000, false,  true }, // drop.
			{   5, 1000, false,  true }, // drop.
			{   6, 1000, false,  true }, // drop.
			{   7, 1000, false,  true }, // drop.
			{   8, 1000, false,  true }, // drop.
			{   9, 1000, false,  true }, // drop.
			{ 120, 1112, false, false },
			{ 121, 1113, false, false },
			{ 122, 1114, false, false },
			{ 123, 1115, false, false },
			{ 124, 1116, false, false },
			{ 125, 1117, false, false },
			{ 126, 1118, false, false },
			{ 127, 1119, false, false },
			{ 128, 1120, false, false },
			{ 129, 1121, false, false },
			{ 130, 1122, false, false },
			{ 131, 1123, false, false },
			{ 132, 1124, false, false },
			{ 133, 1125, false, false },
			{ 134, 1126, false, false },
			{ 135, 1127, false, false },
			{ 136, 1128, false, false },
			{ 137, 1129, false, false },
			{ 138, 1130, false, false },
			{ 139, 1131, false, false }
		};
		// clang-format on

		SeqManager<uint16_t> seqManager(/*initialOutput*/ 1000);
		SeqManager<uint16_t, 15> seqManager2(/*initialOutput*/ 1000);
		validate(seqManager, inputs);
		validate(seqManager2, inputs);
	}

	SECTION("drop many inputs at the beginning (using uint8_t) (with initial output)")
	{
		// clang-format off
		std::vector<TestSeqManagerInput<uint8_t>> inputs =
		{
			{   1, 201, false, false },
			{   2, 200, false,  true }, // drop.
			{   3, 200, false,  true }, // drop.
			{   4, 200, false,  true }, // drop.
			{   5, 200, false,  true }, // drop.
			{   6, 200, false,  true }, // drop.
			{   7, 200, false,  true }, // drop.
			{   8, 200, false,  true }, // drop.
			{   9, 200, false,  true }, // drop.
			{ 120,  56, false, false },
			{ 121,  57, false, false },
			{ 122,  58, false, false },
			{ 123,  59, false, false },
			{ 124,  60, false, false },
			{ 125,  61, false, false },
			{ 126,  62, false, false },
			{ 127,  63, false, false },
			{ 128,  64, false, false },
			{ 129,  65, false, false },
			{ 130,  66, false, false },
			{ 131,  67, false, false },
			{ 132,  68, false, false },
			{ 133,  69, false, false },
			{ 134,  70, false, false },
			{ 135,  71, false, false },
			{ 136,  72, false, false },
			{ 137,  73, false, false },
			{ 138,  74, false, false },
			{ 139,  75, false, false }
		};
		// clang-format on

		SeqManager<uint8_t> seqManager(/*initialOutput*/ 200);
		validate(seqManager, inputs);
	}

	SECTION("receive mixed numbers, sync, drop in range 15 (with initial output)")
	{
		// clang-format off
		std::vector<TestSeqManagerInput<uint16_t>> inputs =
		{
			{     0, 100, false, false },
			{     1, 101, false, false },
			{     2, 102, false, false },
			{     3, 103, false, false },
			{     7, 107, false, false },
			{     6, 100, false,  true }, // drop.
			{     8, 108, false, false },
			{    10, 110, false, false },
			{     9, 109, false, false },
			{    11, 111, false, false },
			{     0, 112,  true, false }, // sync.
			{     2, 114, false, false },
			{     3, 115, false, false },
			{     4, 116, false, false },
			{     5, 117, false, false },
			{     6, 118, false, false },
			{     7, 119, false, false },
			{     8, 120, false, false },
			{     9, 121, false, false },
			{    10, 122, false, false },
			{     9, 100, false,  true }, // drop.
			{    61, 123,  true, false }, // sync.
			{    62, 124, false, false },
			{    63, 125, false, false },
			{    64, 126, false, false },
			{    65, 127, false, false },
			{    11, 128,  true, false }, // sync.
			{    12, 129, false, false },
			{    13, 130, false, false },
			{    14, 131, false, false },
			{    15, 132, false, false },
			{     1, 133,  true, false }, // sync.
			{     2, 134, false, false },
			{     3, 135, false, false },
			{     4, 136, false, false },
			{     5, 137, false, false },
			{ 32767, 138,  true, false }, // sync.
			{ 32768, 139, false, false },
			{ 32769, 140, false, false },
			{     0, 141,  true, false }, // sync.
			{     1, 142, false, false },
			{     3, 100, false,  true }, // drop.
			{     4, 144, false, false },
			{     5, 145, false, false },
			{     6, 146, false, false },
			{     7, 147, false, false }
		};
		// clang-format on

		SeqManager<uint16_t, 15> seqManager(/*initialOutput*/ 100);
		validate(seqManager, inputs);
	}

	SECTION("drop many inputs at the beginning (using uint16_t with high values) (with initial output)")
	{
		// clang-format off
		std::vector<TestSeqManagerInput<uint16_t>> inputs =
		{
			{     1,   201, false, false },
			{     2,   200, false,  true }, // drop.
			{     3,   200, false,  true }, // drop.
			{     4,   200, false,  true }, // drop.
			{     5,   200, false,  true }, // drop.
			{     6,   200, false,  true }, // drop.
			{     7,   200, false,  true }, // drop.
			{     8,   200, false,  true }, // drop.
			{     9,   200, false,  true }, // drop.
			{ 32768, 32960, false, false },
			{ 32769, 32961, false, false },
			{ 32770, 32962, false, false },
			{ 32771, 32963, false, false },
			{ 32772, 32964, false, false },
			{ 32773, 32965, false, false },
			{ 32774, 32966, false, false },
			{ 32775, 32967, false, false },
			{ 32776, 32968, false, false },
			{ 32777, 32969, false, false },
			{ 32778, 32970, false, false },
			{ 32779, 32971, false, false },
			{ 32780, 32972, false, false }
		};
		// clang-format on

		SeqManager<uint16_t> seqManager(/*initialOutput*/ 200);
		validate(seqManager, inputs);
	}

	SECTION("sync and drop some input near max-value (with initial output)")
	{
		// clang-format off
		std::vector<TestSeqManagerInput<uint16_t>> inputs =
		{
			{ 65530, 201,  true, false },
			{ 65531, 202, false, false },
			{ 65532, 203, false, false },
			{ 65533, 200, false, true  },
			{ 65534, 200, false, true  },
			{ 65535, 204, false, false },
			{     0, 205, false, false },
			{     1, 206, false, false },
			{     2, 207, false, false },
			{     3, 208, false, false }
		};
		// clang-format on

		SeqManager<uint16_t> seqManager(/*initialOutput*/ 200);
		validate(seqManager, inputs);
	}

	SECTION(
	  "drop many inputs at the beginning (using uint16_t range 15 with high values) (with initial output)")
	{
		// clang-format off
		std::vector<TestSeqManagerInput<uint16_t>> inputs =
		{
			{     1,   101, false, false },
			{     2,   100, false,  true }, // drop.
			{     3,   100, false,  true }, // drop.
			{     4,   100, false,  true }, // drop.
			{     5,   100, false,  true }, // drop.
			{     6,   100, false,  true }, // drop.
			{     7,   100, false,  true }, // drop.
			{     8,   100, false,  true }, // drop.
			{     9,   100, false,  true }, // drop.
			{ 16384, 16476, false, false },
			{ 16385, 16477, false, false },
			{ 16386, 16478, false, false },
			{ 16387, 16479, false, false },
			{ 16388, 16480, false, false },
			{ 16389, 16481, false, false },
			{ 16390, 16482, false, false },
			{ 16391, 16483, false, false },
			{ 16392, 16484, false, false },
			{ 16393, 16485, false, false },
			{ 16394, 16486, false, false },
			{ 16395, 16487, false, false },
			{ 16396, 16488, false, false }
		};
		// clang-format on

		SeqManager<uint16_t, 15> seqManager(/*initialOutput*/ 100);
		validate(seqManager, inputs);
	}

	SECTION("sync and drop some input near max-value in a 15bit range (with initial output)")
	{
		// clang-format off
		std::vector<TestSeqManagerInput<uint16_t>> inputs =
		{
			{ 32762, 101,  true, false, 32762 },
			{ 32763, 102, false, false, 32763 },
			{ 32764, 103, false, false, 32764 },
			{ 32765, 100, false, true,  32765 },
			{ 32766, 100, false, true,  32766 },
			{ 32767, 104, false, false, 32767 },
			{     0, 105, false, false,     0 },
			{     1, 106, false, false,     1 },
			{     2, 107, false, false,     2 },
			{     3, 108, false, false,     3 }
		};
		// clang-format on

		SeqManager<uint16_t, 15> seqManager(/*initialOutput*/ 100);
		validate(seqManager, inputs);
	}

	SECTION("should update all values during multiple roll overs (with initial output)")
	{
		// clang-format off
		std::vector<TestSeqManagerInput<uint16_t>> inputs =
		{
			{ 0, 101, true, false, 0 },
		};
		for (uint16_t j = 0; j < 3; ++j) {
			for (uint16_t i = 1; i < std::numeric_limits<uint16_t>::max(); ++i) {
				const uint16_t output = (i + 1 + 100) & std::numeric_limits<uint16_t>::max();
				inputs.emplace_back( i, output, false, false, i );
			}
		}
		// clang-format on

		SeqManager<uint16_t> seqManager(/*initialOutput*/ 100);
		validate(seqManager, inputs);
	}

	SECTION("should update all values during multiple roll overs (15 bits range) (with initial output)")
	{
		// clang-format off
		std::vector<TestSeqManagerInput<uint16_t>> inputs =
		{
			{ 0, 101, true, false, 0, },
		};
		for (uint16_t j = 0; j < 3; ++j) {
			for (uint16_t i = 1; i < MaxNumberFor15Bits; ++i) {
				const uint16_t output = (i + 1 + 100) & MaxNumberFor15Bits;
				inputs.emplace_back( i, output, false, false, i );
			}
		}
		// clang-format on

		SeqManager<uint16_t, 15> seqManager(/*initialOutput*/ 100);
		validate(seqManager, inputs);
	}

	SECTION(
	  "should produce same output for same old input before drop (15 bits range) (with initial output)")
	{
		// clang-format off
		std::vector<TestSeqManagerInput<uint16_t>> inputs =
		{
			{ 10, 10001, true,  false }, // sync.
			{ 11, 10002, false, false },
			{ 12, 10003, false, false },
			{ 13, 10004, false, false },
			{ 14, 10000, false, true  }, // drop.
			{ 15, 10005, false, false },
			{ 12, 10003, false, false }
		};
		// clang-format on

		SeqManager<uint16_t, 15> seqManager(/*initialOutput*/ 10000);
		validate(seqManager, inputs);
	}

	SECTION("should properly clean previous cycle drops (with initial output)")
	{
		// clang-format off
		std::vector<TestSeqManagerInput<uint8_t>> inputs =
		{
			{ 1, 3, false, false },
			{ 2, 2, false, true  }, // Drop.
			{ 3, 4, false, false },
			{ 4, 5, false, false },
			{ 5, 6, false, false },
			{ 6, 7, false, false },
			{ 7, 0, false, false },
			{ 0, 1, false, false },
			{ 1, 2, false, false },
			{ 2, 3, false, false },
			{ 3, 4, false, false }
		};
		// clang-format on

		SeqManager<uint8_t, 3> seqManager(/*initialOutput*/ 2);
		validate(seqManager, inputs);
	}

	SECTION("dropped inputs to be removed going out of range, 1. (with initial output)")
	{
		// clang-format off
		std::vector<TestSeqManagerInput<uint16_t>> inputs =
		{
			{ 36964, 46964, false, false },
			{ 25923, 10000, false, true  }, // Drop.
			{ 25701, 35701, false, false },
			{ 17170, 10000, false, true  }, // Drop.
			{ 25923, 35923, false, false },
			{  4728, 10000, false, true  }, // Drop.
			{ 17170, 27170, false, false },
			{ 30738, 10000, false, true  }, // Drop.
			{  4728, 14728, false, false },
			{  4806, 10000, false, true  }, // Drop.
			{ 30738, 40738, false, false },
			{ 50886, 10000, false, true  }, // Drop.
			{  4806, 14805, false, false }, // Previously dropped.
			{ 50774, 10000, false, true  }, // Drop.
			{ 50886, 14805, false, false }, // Previously dropped.
			{ 22136, 10000, false, true  }, // Drop.
			{ 50774, 60773, false, false },
			{ 30910, 10000, false, true  }, // Drop.
			{ 22136, 60773, false, false }, // Previously dropped.
			{ 48862, 10000, false, true  }, // Drop.
			{ 30910, 40909, false, false },
			{ 56832, 10000, false, true  }, // Drop.
			{ 48862, 58861, false, false },
			{     2, 10000, false, true  }, // Drop.
			{ 56832, 58861, false, false }, // Previously dropped.
			{   530, 10000, false, true  }, // Drop.
			{     2, 58861, false, false }, // Previously dropped.
		};
		// clang-format on

		SeqManager<uint16_t> seqManager(/*initialOutput*/ 10000);
		validate(seqManager, inputs);
	}

	SECTION("dropped inputs to be removed go out of range, 2. (with initial output)")
	{
		// clang-format off
		std::vector<TestSeqManagerInput<uint16_t>> inputs =
		{
			{ 36960, 37060, false, false },
			{  3328,   100, false, true  }, // Drop.
			{ 24589, 24688, false, false },
			{   120,   100, false, true  }, // Drop.
			{  3328, 24688, false, false }, // Previously dropped.
			{ 30848,   100, false, true  }, // Drop.
			{   120,   220, false, false },
		};
		// clang-format on

		SeqManager<uint16_t> seqManager(/*initialOutput*/ 100);
		validate(seqManager, inputs);
	}

	SECTION("dropped inputs to be removed go out of range, 3. (with initial output)")
	{
		// clang-format off
		std::vector<TestSeqManagerInput<uint16_t>> inputs =
		{
			{ 36964, 37964, false, false },
			{ 65396 , 1000, false, true  }, // Drop.
			{ 25855, 26854, false, false },
			{ 29793 , 1000, false, true  }, // Drop.
			{ 65396, 26854, false, false }, // Previously dropped.
			{ 25087,  1000,  false, true  }, // Drop.
			{ 29793, 26854, false, false }, // Previously dropped.
			{ 65535 , 1000, false, true  }, // Drop.
			{ 25087, 26086, false, false },
		};
		// clang-format on

		SeqManager<uint16_t> seqManager(/*initialOutput*/ 1000);
		validate(seqManager, inputs);
	}
}
