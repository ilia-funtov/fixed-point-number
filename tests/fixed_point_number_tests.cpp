// Copyright (c) 2022 Ilia Funtov
// Distributed under the Boost Software License, Version 1.0. (http://www.boost.org/LICENSE_1_0.txt)

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cmath>
#include <functional>
#include <limits>
#include <type_traits>
#include <vector>

#include <fixed_point_number.hpp>

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

#include "unit_tests_common.hpp"

namespace fixed_point_arithmetic
{		
	using namespace unit_tests_common;

	using template_test_types = std::tuple<std::int8_t, std::int16_t, std::int32_t, std::int64_t>;

	using list_of_integer_types = type_list<std::int8_t, std::int16_t, std::int32_t, std::int64_t>;
	using list_of_floating_types = type_list<float, double, long double>;
	using list_of_types_to_test = list_of_integer_types;

	template <typename container_t, typename element_t>
	struct type_can_contain : std::integral_constant<bool, sizeof(container_t) >= sizeof(element_t)>
	{
		static_assert(std::is_integral<container_t>::value && std::is_integral<element_t>::value, "type_can_contain is applicable only for integral types.");
	};

	template<typename value_t, value_t begin, value_t end>
	class for_each_in_range_descending : private for_each_in_range_descending<value_t, begin - 1, end>
	{
	public:
		static_assert(begin >= end, "Begin value must be greater or equal than end value.");

		template <typename handler_t>
		static void process(handler_t && handler)
		{
			handler.template operator()<value_t, begin>();
			parent_type::process(std::forward<handler_t>(handler));
		}

	private:
		using parent_type = for_each_in_range_descending<value_t, begin - 1, end>;
	};

	template<typename value_t, value_t value>
	class for_each_in_range_descending<value_t, value, value>
	{
	public:
		template <typename handler_t>
		static void process(handler_t && handler)
		{
			handler.template operator()<value_t, value>();
		}
	};

	template <template <typename value_t, unsigned int fraction_digits_num> typename test_t>
	class fixed_point_tester
	{
	public:
		template <typename value_t>
		fixed_point_tester & operator ()()
		{
			for_each_in_range_descending<value_t, max_decimal_digits_num<value_t>::value - 1, 0>::
				process(fixed_point_digits_num_tester());

			return *this;
		}
	private:
		class fixed_point_digits_num_tester
		{
		public:
			template <typename value_t, value_t value>
			fixed_point_digits_num_tester & operator() ()
			{
				test_t<value_t, value>();
				return *this;
			}
		};
	};

	template <typename value_t>
	value_t get_opposite_value(value_t value)
	{
		static_assert(std::is_signed<value_t>::value, "get_opposite_value: only signed types are supported.");

		if (value == 0)
			return 0;

		auto result = static_cast<value_t>(-value);
		if (result == value)
		{
			result = (result < 0) ? result + 1 : result - 1;
			result = -result;
		}

		REQUIRE(result != value);

		return result;
	}

	template <typename value_t, unsigned int fraction_digits_num, typename test_value_t>
	bool is_equal_floating_point_values(
		test_value_t a,
		test_value_t b,
		test_value_t threshold_delta = std::numeric_limits<test_value_t>::round_error() * 3 / static_cast<test_value_t>(decimal_scale<value_t, fraction_digits_num>::value)) // 3 operands are affected by round
	{
		const auto delta = std::abs(a - b);		
		const auto result = (delta != 0) ? delta < threshold_delta : true;
		return result;
	}

	template <typename value_t, unsigned int fraction_digits_num, typename test_value_t>
	std::vector<test_value_t> generate_integer_test_values()
	{
		static_assert(std::numeric_limits<value_t>::max() >= std::numeric_limits<test_value_t>::max() &&
			std::numeric_limits<value_t>::min() <= std::numeric_limits<test_value_t>::min(),
			"Values of test value type is out of range of fixed point storage type.");

		const auto scale_factor = fixed_point_number<value_t, fraction_digits_num>::scale_value;
		const auto min_value = std::numeric_limits<test_value_t>::min() / scale_factor;
		const auto max_value = std::numeric_limits<test_value_t>::max() / scale_factor;

		std::vector<test_value_t> result;
		result.push_back(0);
		result.push_back(min_value);
		result.push_back(max_value);
		result.push_back(min_value / 2);
		result.push_back(max_value / 2);

		for (test_value_t x = 1; x < max_value / 2; x *= 2)
		{
			result.push_back(x);
		}

		for (test_value_t x = -1; x > min_value / 2; x *= 2)
		{
			result.push_back(x);
		}

		return result;
	}

	template <typename value_t, unsigned int fraction_digits_num, typename floating_t>
	std::vector<floating_t> generate_floating_point_test_values()
	{
		std::vector<floating_t> result;

		const auto scale_factor = static_cast<floating_t>(fixed_point_number<value_t, fraction_digits_num>::scale_value);
		const auto min_value = std::numeric_limits<value_t>::min();
		const auto max_value = std::numeric_limits<value_t>::max();

		result.push_back(static_cast<floating_t>(0));
		result.push_back(static_cast<floating_t>(max_value / 2) / scale_factor);
		result.push_back(static_cast<floating_t>(min_value / 2) / scale_factor);

		for (value_t x = 1; x < max_value / 2; x *= 2)
		{
			result.push_back(static_cast<floating_t>(x) / scale_factor);
		}

		for (value_t x = -1; x > min_value / 2; x *= 2)
		{
			result.push_back(static_cast<floating_t>(x) / scale_factor);
		}

		return result;
	}

	template <unsigned int fraction_digits_num, typename sample_t>
	sample_t value_for_mult_div_test(sample_t x)
	{
		using largest_int_test_type = std::int64_t;

		const auto scale_value = static_cast<sample_t>(decimal_scale<largest_int_test_type, fraction_digits_num>::value);
		auto abs_value = std::abs(x);
		auto root = std::sqrt(abs_value);
		while (root * root * scale_value * scale_value >= static_cast<sample_t>(std::numeric_limits<largest_int_test_type>::max()))
		{
			abs_value /= 2;
			root = std::sqrt(abs_value);
		}

		auto value = root * root;
		value = (x < 0) ? -value : value;

		return static_cast<sample_t>(value);
	}

	template <typename value_t, unsigned int fraction_digits_num, typename floating_t>
	std::vector<floating_t> generate_floating_point_test_values_for_mult_div()
	{
		std::vector<floating_t> result;

		const auto scale_factor = static_cast<floating_t>(fixed_point_number<value_t, fraction_digits_num>::scale_value);
		const auto min_value = std::numeric_limits<value_t>::min();
		const auto max_value = std::numeric_limits<value_t>::max();

		result.push_back(static_cast<floating_t>(0));
		result.push_back(value_for_mult_div_test<fraction_digits_num>(static_cast<floating_t>(max_value / 2) / scale_factor));
		result.push_back(value_for_mult_div_test<fraction_digits_num>(static_cast<floating_t>(min_value / 2) / scale_factor));

		for (value_t x = 1; x < max_value / 2; x *= 2)
		{
			result.push_back(value_for_mult_div_test<fraction_digits_num>(static_cast<floating_t>(x) / scale_factor));
		}

		for (value_t x = -1; x > min_value / 2; x *= 2)
		{
			result.push_back(value_for_mult_div_test<fraction_digits_num>(static_cast<floating_t>(x) / scale_factor));
		}

		return result;
	}

	template <typename value_t, unsigned int fraction_digits_num, typename test_value_t>
	std::vector<test_value_t> generate_integer_point_test_values_for_mult_div()
	{
		static_assert(std::is_signed<value_t>::value && std::is_signed<test_value_t>::value, "Only signed types are supported.");
		using common_type = typename std::common_type<value_t, test_value_t>::type;

		std::vector<test_value_t> result;

		const auto scale_value = fixed_point_number<value_t, fraction_digits_num>::scale_value;

		const auto min_value = std::min(
			static_cast<common_type>(std::numeric_limits<test_value_t>::min()),
			static_cast<common_type>(std::numeric_limits<value_t>::min()));

		const auto max_value = std::max(
			static_cast<common_type>(std::numeric_limits<test_value_t>::max()),
			static_cast<common_type>(std::numeric_limits<value_t>::max()));

		const auto root_min = -static_cast<decltype(min_value)>(std::sqrt(std::abs(min_value / scale_value)));
		const auto root_max = static_cast<decltype(max_value)>(std::sqrt(std::abs(max_value / scale_value)));

		result.push_back(0);

		for (value_t x = 1; x < root_max / 2; x *= 2)
		{
			result.push_back(static_cast<test_value_t>(x * x));
		}

		for (value_t x = -1; x > root_min / 2; x *= 2)
		{
			result.push_back(static_cast<test_value_t>(-x * x));
		}

		return result;
	}

	template <typename value_t, unsigned int fraction_digits_num>
	class fixed_point_construction_test
	{
	public:
		fixed_point_construction_test()
		{
			const fixed_point_number<value_t, fraction_digits_num> default_constructed_1;
			const fixed_point_number<value_t, fraction_digits_num> default_constructed_2;

			REQUIRE(default_constructed_1 == default_constructed_2);

			list_of_integer_types::for_each_type(integer_test());
			list_of_floating_types::for_each_type(floating_point_test());
		}
	private:
		class floating_point_test
		{
		public:
			template <typename test_value_t>
			floating_point_test & operator ()()
			{
				const auto test_values = generate_floating_point_test_values<value_t, fraction_digits_num, test_value_t>();
				REQUIRE(!test_values.empty());

				for (const auto & test_value : test_values)
				{
					test_fixed_point_construction_for_floating_point_value(test_value);
				}

				return *this;
			}
		private:
			template <typename test_value_t>
			static void test_fixed_point_construction_for_floating_point_value(test_value_t test_value)
			{
				static_assert(std::is_floating_point<test_value_t>::value, "test_value_t must be a floating point type.");

				const fixed_point_number<value_t, fraction_digits_num> implicit_constructed = test_value;
				REQUIRE((is_equal_floating_point_values<value_t, fraction_digits_num>(static_cast<test_value_t>(implicit_constructed), test_value)));

				const auto copy_constructed = implicit_constructed;
				REQUIRE(copy_constructed == implicit_constructed);
				REQUIRE(copy_constructed == test_value);
				REQUIRE((is_equal_floating_point_values<value_t, fraction_digits_num>(static_cast<test_value_t>(copy_constructed), test_value)));

				auto copy = copy_constructed;
				const auto move_constructed = std::move(copy);
				REQUIRE(move_constructed == test_value);
				REQUIRE((is_equal_floating_point_values<value_t, fraction_digits_num>(static_cast<test_value_t>(move_constructed), test_value)));
			}
		}; // class floating_point_test

		class integer_test
		{
		public:
			template <typename test_value_t>
			typename std::enable_if<type_can_contain<value_t, test_value_t>::value, integer_test &>::type
			operator ()()
			{
				const auto test_values = generate_integer_test_values<value_t, fraction_digits_num, test_value_t>();
				REQUIRE(!test_values.empty());

				for (const auto & test_value : test_values)
				{
					test_fixed_point_construction_for_value(test_value);
				}

				return *this;
			}

			template <typename test_value_t>
			typename std::enable_if<!type_can_contain<value_t, test_value_t>::value, integer_test &>::type
				operator ()()
			{
				// do nothing
				return *this;
			}
		private:
			template <typename test_value_t>
			static void test_fixed_point_construction_for_value(test_value_t test_value)
			{
				static_assert(std::is_integral<test_value_t>::value, "test_value_t must be an integral type.");

				const fixed_point_number<value_t, fraction_digits_num> implicit_constructed = test_value;
				REQUIRE(static_cast<test_value_t>(implicit_constructed) == test_value);

				const auto copy_constructed = implicit_constructed;
				REQUIRE(copy_constructed == implicit_constructed);
				REQUIRE(copy_constructed == test_value);
				REQUIRE(static_cast<test_value_t>(copy_constructed) == test_value);

				auto copy = copy_constructed;
				const auto move_constructed = std::move(copy);
				REQUIRE(move_constructed == test_value);
				REQUIRE(static_cast<test_value_t>(move_constructed) == test_value);
			}
		}; // class integer_test
	}; // class fixed_point_construction_test

	template <typename value_t, unsigned int fraction_digits_num>
	class fixed_point_assign_test
	{
	public:
		fixed_point_assign_test()
		{
			list_of_integer_types::for_each_type(integer_test());
			list_of_floating_types::for_each_type(floating_point_test());
		}
	private:
		class floating_point_test
		{
		public:
			template <typename test_value_t>
			floating_point_test & operator ()()
			{
				const auto test_values = generate_floating_point_test_values<value_t, fraction_digits_num, test_value_t>();
				REQUIRE(!test_values.empty());

				fixed_point_number<value_t, fraction_digits_num> value;
				for (const auto & test_value : test_values)
				{
					test_fixed_point_assignment_for_floating_point_value(value, test_value);
				}

				return *this;
			}
		private:
			template <typename test_value_t>
			static void test_fixed_point_assignment_for_floating_point_value(
				fixed_point_number<value_t, fraction_digits_num> & value,
				test_value_t test_value)
			{
				static_assert(std::is_floating_point<test_value_t>::value, "test_value_t must be a floating point type.");

				value = test_value;
				REQUIRE(value == value);
				REQUIRE((is_equal_floating_point_values<value_t, fraction_digits_num>(static_cast<test_value_t>(value), test_value)));

				value = value; // self-assignment test
				REQUIRE((is_equal_floating_point_values<value_t, fraction_digits_num>(static_cast<test_value_t>(value), test_value)));

				value = std::move(value); // move self-assignment test
				REQUIRE((is_equal_floating_point_values<value_t, fraction_digits_num>(static_cast<test_value_t>(value), test_value)));
			}
		}; // class floating_point_test

		class integer_test
		{
		public:
			template <typename test_value_t>
			typename std::enable_if<type_can_contain<value_t, test_value_t>::value, integer_test &>::type
				operator ()()
			{
				const auto test_values = generate_integer_test_values<value_t, fraction_digits_num, test_value_t>();
				REQUIRE(!test_values.empty());

				fixed_point_number<value_t, fraction_digits_num> value;
				for (const auto & test_value : test_values)
				{
					test_fixed_point_assignment_for_value(value, test_value);
				}

				return *this;
			}

			template <typename test_value_t>
			typename std::enable_if<!type_can_contain<value_t, test_value_t>::value, integer_test &>::type
				operator ()()
			{
				// do nothing
				return *this;
			}
		private:
			template <typename test_value_t>
			static void test_fixed_point_assignment_for_value(
				fixed_point_number<value_t, fraction_digits_num> & value,
				test_value_t test_value)
			{
				static_assert(std::is_integral<test_value_t>::value, "test_value_t must be an integral type.");

				value = test_value;
				REQUIRE(value == value);
				REQUIRE(value == test_value);

				value = value; // self-assignment test
				REQUIRE(value == test_value);

				value = std::move(value); // move self-assignment test
				REQUIRE(value == test_value);
			}
		}; // class integer_test
	}; // class fixed_point_assign_test

	template <typename value_t, unsigned int fraction_digits_num>
	class fixed_point_add_test
	{
	public:
		fixed_point_add_test()
		{
			list_of_integer_types::for_each_type(integer_test());
			list_of_floating_types::for_each_type(floating_point_test());
		}
	private:
		class floating_point_test
		{
		public:
			template <typename test_value_t>
			floating_point_test & operator ()()
			{
				const auto test_values = generate_floating_point_test_values<value_t, fraction_digits_num, test_value_t>();
				REQUIRE(!test_values.empty());

				for (const auto & test_value : test_values)
				{
					test_fixed_point_add_for_floating_point_value(test_value);
				}

				return *this;
			}
		private:
			template <typename test_value_t>
			static void test_fixed_point_add_for_floating_point_value(test_value_t test_value)
			{
				static_assert(std::is_floating_point<test_value_t>::value, "test_value_t must be a floating point type.");
				
				const auto test_value_2 = -test_value;
				const auto result_expected = test_value + test_value_2;

				fixed_point_number<value_t, fraction_digits_num> a = test_value;
				fixed_point_number<value_t, fraction_digits_num> b = test_value_2;
				
				const auto c = a + b;
				const auto d = test_value + b;
				const auto e = a + test_value_2;
				const auto f = b + a;
				const auto g = b + test_value;
				const auto h = test_value_2 + a;
				a += b;
				b += test_value;

				REQUIRE((is_equal_floating_point_values<value_t, fraction_digits_num>(static_cast<test_value_t>(a), result_expected)));
				REQUIRE((is_equal_floating_point_values<value_t, fraction_digits_num>(static_cast<test_value_t>(b), result_expected)));
				REQUIRE((is_equal_floating_point_values<value_t, fraction_digits_num>(static_cast<test_value_t>(c), result_expected)));
				REQUIRE((is_equal_floating_point_values<value_t, fraction_digits_num>(static_cast<test_value_t>(d), result_expected)));
				REQUIRE((is_equal_floating_point_values<value_t, fraction_digits_num>(static_cast<test_value_t>(e), result_expected)));
				REQUIRE((is_equal_floating_point_values<value_t, fraction_digits_num>(static_cast<test_value_t>(f), result_expected)));
				REQUIRE((is_equal_floating_point_values<value_t, fraction_digits_num>(static_cast<test_value_t>(g), result_expected)));
				REQUIRE((is_equal_floating_point_values<value_t, fraction_digits_num>(static_cast<test_value_t>(h), result_expected)));
			}
		}; // class floating_point_test

		class integer_test
		{
		public:
			template <typename test_value_t>
			typename std::enable_if<type_can_contain<value_t, test_value_t>::value, integer_test &>::type
				operator ()()
			{
				const auto test_values = generate_integer_test_values<value_t, fraction_digits_num, test_value_t>();
				REQUIRE(!test_values.empty());

				for (const auto & test_value : test_values)
				{
					test_fixed_point_add_for_value(test_value);
				}

				return *this;
			}

			template <typename test_value_t>
			typename std::enable_if<!type_can_contain<value_t, test_value_t>::value, integer_test &>::type
				operator ()()
			{
				// do nothing
				return *this;
			}
		private:
			template <typename test_value_t>
			static void test_fixed_point_add_for_value(test_value_t test_value)
			{
				static_assert(std::is_integral<test_value_t>::value, "test_value_t must be an integral type.");

				auto test_value_2 = get_opposite_value(test_value);
				const auto result_expected = test_value + test_value_2;

				fixed_point_number<value_t, fraction_digits_num> a = test_value;
				fixed_point_number<value_t, fraction_digits_num> b = test_value_2;

				const auto c = a + b;
				const auto d = test_value + b;
				const auto e = a + test_value_2;
				const auto f = b + a;
				const auto g = b + test_value;
				const auto h = test_value_2 + a;
				a += b;
				b += test_value;

				REQUIRE(static_cast<test_value_t>(a) == result_expected);
				REQUIRE(static_cast<test_value_t>(b) == result_expected);
				REQUIRE(static_cast<test_value_t>(c) == result_expected);
				REQUIRE(static_cast<test_value_t>(d) == result_expected);
				REQUIRE(static_cast<test_value_t>(e) == result_expected);
				REQUIRE(static_cast<test_value_t>(f) == result_expected);
				REQUIRE(static_cast<test_value_t>(g) == result_expected);
				REQUIRE(static_cast<test_value_t>(h) == result_expected);
			}
		}; // class integer_test
	}; // class fixed_point_add_test

	template <typename value_t, unsigned int fraction_digits_num>
	class fixed_point_subtract_test
	{
	public:
		fixed_point_subtract_test()
		{
			list_of_integer_types::for_each_type(integer_test());
			list_of_floating_types::for_each_type(floating_point_test());
		}
	private:
		class floating_point_test
		{
		public:
			template <typename test_value_t>
			floating_point_test & operator ()()
			{
				const auto test_values = generate_floating_point_test_values<value_t, fraction_digits_num, test_value_t>();
				REQUIRE(!test_values.empty());

				for (const auto & test_value : test_values)
				{
					test_fixed_point_subtract_for_floating_point_value(test_value);
				}

				return *this;
			}
		private:
			template <typename test_value_t>
			static void test_fixed_point_subtract_for_floating_point_value(test_value_t test_value)
			{
				static_assert(std::is_floating_point<test_value_t>::value, "test_value_t must be a floating point type.");

				const auto test_value_2 = test_value / 2;
				const auto result_expected = test_value - test_value_2;
				const auto result_expected_2 = test_value_2 - test_value;

				fixed_point_number<value_t, fraction_digits_num> a = test_value;
				fixed_point_number<value_t, fraction_digits_num> b = test_value_2;
				fixed_point_number<value_t, fraction_digits_num> j = test_value;

				const auto c = a - b;
				const auto d = test_value - b;
				const auto e = a - test_value_2;
				const auto f = b - a;
				const auto g = b - test_value;
				const auto h = test_value_2 - a;

				a -= b;
				b -= j;
				j -= j;

				REQUIRE((is_equal_floating_point_values<value_t, fraction_digits_num>(static_cast<test_value_t>(a), result_expected)));
				REQUIRE((is_equal_floating_point_values<value_t, fraction_digits_num>(static_cast<test_value_t>(b), result_expected_2)));
				REQUIRE((is_equal_floating_point_values<value_t, fraction_digits_num>(static_cast<test_value_t>(c), result_expected)));
				REQUIRE((is_equal_floating_point_values<value_t, fraction_digits_num>(static_cast<test_value_t>(d), result_expected)));
				REQUIRE((is_equal_floating_point_values<value_t, fraction_digits_num>(static_cast<test_value_t>(e), result_expected)));
				REQUIRE((is_equal_floating_point_values<value_t, fraction_digits_num>(static_cast<test_value_t>(f), result_expected_2)));
				REQUIRE((is_equal_floating_point_values<value_t, fraction_digits_num>(static_cast<test_value_t>(g), result_expected_2)));
				REQUIRE((is_equal_floating_point_values<value_t, fraction_digits_num>(static_cast<test_value_t>(h), result_expected_2)));
				REQUIRE((is_equal_floating_point_values<value_t, fraction_digits_num>(static_cast<test_value_t>(j), test_value_t(0))));
			}
		}; // class floating_point_test

		class integer_test
		{
		public:
			template <typename test_value_t>
			typename std::enable_if<type_can_contain<value_t, test_value_t>::value, integer_test &>::type
				operator ()()
			{
				const auto test_values = generate_integer_test_values<value_t, fraction_digits_num, test_value_t>();
				REQUIRE(!test_values.empty());

				for (const auto & test_value : test_values)
				{
					test_fixed_point_subtract_for_value(test_value);
				}

				return *this;
			}

			template <typename test_value_t>
			typename std::enable_if<!type_can_contain<value_t, test_value_t>::value, integer_test &>::type
				operator ()()
			{
				// do nothing
				return *this;
			}
		private:
			template <typename test_value_t>
			static void test_fixed_point_subtract_for_value(test_value_t test_value)
			{
				static_assert(std::is_integral<test_value_t>::value, "test_value_t must be an integral type.");

				auto test_value_2 = static_cast<test_value_t>(test_value / 2);
				const auto result_expected = test_value - test_value_2;
				const auto result_expected_2 = test_value_2 - test_value;

				fixed_point_number<value_t, fraction_digits_num> a = test_value;
				fixed_point_number<value_t, fraction_digits_num> b = test_value_2;
				fixed_point_number<value_t, fraction_digits_num> j = test_value;

				const auto c = a - b;
				const auto d = test_value - b;
				const auto e = a - test_value_2;
				const auto f = b - a;
				const auto g = b - test_value;
				const auto h = test_value_2 - a;

				a -= b;
				b -= j;
				j -= j;

				REQUIRE(static_cast<test_value_t>(a) == result_expected);
				REQUIRE(static_cast<test_value_t>(b) == result_expected_2);
				REQUIRE(static_cast<test_value_t>(c) == result_expected);
				REQUIRE(static_cast<test_value_t>(d) == result_expected);
				REQUIRE(static_cast<test_value_t>(e) == result_expected);
				REQUIRE(static_cast<test_value_t>(f) == result_expected_2);
				REQUIRE(static_cast<test_value_t>(g) == result_expected_2);
				REQUIRE(static_cast<test_value_t>(h) == result_expected_2);
				REQUIRE(static_cast<test_value_t>(j) == 0);
			}
		}; // class integer_test
	}; // class fixed_point_subtract_test

	template <typename value_t, unsigned int fraction_digits_num>
	class fixed_point_multiply_divide_test
	{
	public:
		fixed_point_multiply_divide_test()
		{
			list_of_integer_types::for_each_type(integer_test());
			list_of_floating_types::for_each_type(floating_point_test());
		}
	private:
		class floating_point_test
		{
		public:
			template <typename test_value_t>
			floating_point_test & operator ()()
			{
				const auto test_values = generate_floating_point_test_values_for_mult_div<value_t, fraction_digits_num, test_value_t>();
				REQUIRE(!test_values.empty());

				for (const auto & test_value : test_values)
				{
					test_fixed_point_multiply_divide_for_floating_point_value(test_value);
				}

				return *this;
			}
		private:
			template <typename test_value_t>
			static void test_fixed_point_multiply_divide_for_floating_point_value(test_value_t test_value)
			{
				static_assert(std::is_floating_point<test_value_t>::value, "test_value_t must be a floating point type.");

				const auto root = std::sqrt(std::abs(test_value));
				const auto minus_root = -root;

				const auto abs_value = std::abs(test_value);
				const auto expected_1 = abs_value;
				const auto expected_2 = -expected_1;

				const auto scale_value = fixed_point_number<value_t, fraction_digits_num>::scale_value;

				const auto round_error = std::numeric_limits<test_value_t>::round_error();

				const auto expected_delta_mult = 
					(round_error + 2 * root * round_error) / static_cast<test_value_t>(scale_value) +
					std::numeric_limits<test_value_t>::epsilon() * (2 * root + abs_value);

				const auto expected_delta_div = 
					(2 * round_error + round_error / abs_value) / static_cast<test_value_t>(scale_value) +
					std::numeric_limits<test_value_t>::epsilon() * (root + 2 * abs_value);

				fixed_point_number<value_t, fraction_digits_num> a = root;
				fixed_point_number<value_t, fraction_digits_num> b = root;
				fixed_point_number<value_t, fraction_digits_num> c = minus_root;

				const auto d = a * b;
				const auto e = b * a;

				const auto f = a * c;
				const auto g = c * a;
				const auto h = c * c;

				const auto j = a * root;
				const auto i = a * minus_root;
				const auto k = root * a;
				const auto l = minus_root * a;

				a *= a;
				b *= c;
				c *= minus_root;

				fixed_point_number<value_t, fraction_digits_num> m = minus_root;
				m *= m;

				const auto check_expected_1 = [&](const fixed_point_number<value_t, fraction_digits_num> value)
				{
					REQUIRE((is_equal_floating_point_values<value_t, fraction_digits_num>(static_cast<test_value_t>(value), expected_1, expected_delta_mult)));
				};

				const auto check_expected_2 = [&](const fixed_point_number<value_t, fraction_digits_num> value)
				{
					REQUIRE((is_equal_floating_point_values<value_t, fraction_digits_num>(static_cast<test_value_t>(value), expected_2, expected_delta_mult)));
				};

				check_expected_1(a);
				check_expected_2(b);
				check_expected_1(c);
				check_expected_1(d);
				check_expected_1(e);
				check_expected_2(f);
				check_expected_2(g);
				check_expected_1(h);
				check_expected_1(j);
				check_expected_1(k);
				check_expected_2(l);
				check_expected_1(m);

				if (root == 0)
					return;

				const auto check_expected_root = [&](const fixed_point_number<value_t, fraction_digits_num> value)
				{
					REQUIRE((is_equal_floating_point_values<value_t, fraction_digits_num>(static_cast<test_value_t>(value), root, expected_delta_div)));
				};

				const auto check_expected_minus_root = [&](const fixed_point_number<value_t, fraction_digits_num> value)
				{
					REQUIRE((is_equal_floating_point_values<value_t, fraction_digits_num>(static_cast<test_value_t>(value), -root, expected_delta_div)));
				};

				fixed_point_number<value_t, fraction_digits_num> n = expected_1;
				fixed_point_number<value_t, fraction_digits_num> o = expected_2;
				fixed_point_number<value_t, fraction_digits_num> p = n / root;
				fixed_point_number<value_t, fraction_digits_num> r = n / -root;
				fixed_point_number<value_t, fraction_digits_num> s = o / root;
				fixed_point_number<value_t, fraction_digits_num> t = o / -root;

				n /= root;
				o /= -root;

				check_expected_root(n);
				check_expected_root(o);
				check_expected_root(p);
				check_expected_minus_root(r);
				check_expected_minus_root(s);
				check_expected_root(t);
			}
		}; // class floating_point_test

		class integer_test
		{
		public:
			template <typename test_value_t>
			typename std::enable_if<type_can_contain<value_t, test_value_t>::value, integer_test &>::type
				operator ()()
			{
				const auto test_values = generate_integer_point_test_values_for_mult_div<value_t, fraction_digits_num, test_value_t>();
				REQUIRE(!test_values.empty());

				for (const auto & test_value : test_values)
				{
					test_fixed_point_multiply_divide_for_value(test_value);
				}

				return *this;
			}

			template <typename test_value_t>
			typename std::enable_if<!type_can_contain<value_t, test_value_t>::value, integer_test &>::type
				operator ()()
			{
				// do nothing
				return *this;
			}
		private:
			template <typename test_value_t>
			static void test_fixed_point_multiply_divide_for_value(test_value_t test_value)
			{
				static_assert(std::is_integral<test_value_t>::value, "test_value_t must be an integral type.");

				const auto root = static_cast<test_value_t>(std::sqrt(std::abs(test_value)));
				const auto minus_root = static_cast<test_value_t>(-root);

				const auto expected_1 = static_cast<test_value_t>(std::abs(test_value));
				const auto expected_2 = static_cast<test_value_t>(-expected_1);

				fixed_point_number<value_t, fraction_digits_num> a = root;
				fixed_point_number<value_t, fraction_digits_num> b = root;
				fixed_point_number<value_t, fraction_digits_num> c = minus_root;

				const auto d = a * b;
				const auto e = b * a;

				const auto f = a * c;
				const auto g = c * a;
				const auto h = c * c;

				const auto j = a * root;
				const auto i = a * minus_root;
				const auto k = root * a;
				const auto l = minus_root * a;

				a *= a;
				b *= c;
				c *= minus_root;

				fixed_point_number<value_t, fraction_digits_num> m = minus_root;
				m *= m;

				const auto check_expected_1 = [&](const fixed_point_number<value_t, fraction_digits_num> & value)
				{
					REQUIRE(static_cast<decltype(expected_1)>(value) == expected_1);
				};

				const auto check_expected_2 = [&](const fixed_point_number<value_t, fraction_digits_num> & value)
				{
					REQUIRE(static_cast<decltype(expected_2)>(value) == expected_2);
				};

				check_expected_1(a);
				check_expected_2(b);
				check_expected_1(c);
				check_expected_1(d);
				check_expected_1(e);
				check_expected_2(f);
				check_expected_2(g);
				check_expected_1(h);
				check_expected_1(j);
				check_expected_1(k);
				check_expected_2(l);
				check_expected_1(m);

				if (root == 0)
					return;

				const auto check_expected_root = [&](const fixed_point_number<value_t, fraction_digits_num> & value)
				{
					REQUIRE(static_cast<decltype(root)>(value) == root);
				};

				const auto check_expected_minus_root = [&](const fixed_point_number<value_t, fraction_digits_num> & value)
				{
					REQUIRE(static_cast<decltype(root)>(value) == -root);
				};

				fixed_point_number<value_t, fraction_digits_num> n = expected_1;
				fixed_point_number<value_t, fraction_digits_num> o = expected_2;
				fixed_point_number<value_t, fraction_digits_num> p = n / static_cast<value_t>(root);
				fixed_point_number<value_t, fraction_digits_num> r = n / static_cast<value_t>(-root);
				fixed_point_number<value_t, fraction_digits_num> s = o / static_cast<value_t>(root);
				fixed_point_number<value_t, fraction_digits_num> t = o / static_cast<value_t>(-root);

				n /= static_cast<value_t>(root);
				o /= static_cast<value_t>(-root);

				check_expected_root(n);
				check_expected_root(o);
				check_expected_root(p);
				check_expected_minus_root(r);
				check_expected_minus_root(s);
				check_expected_root(t);
			}
		}; // class integer_test
	}; // class fixed_point_multiply_divide_test

	template <typename value_t, unsigned int fraction_digits_num>
	class fixed_point_out_of_range_test
	{
	public:
		fixed_point_out_of_range_test()
		{
			const auto max = std::numeric_limits<value_t>::max();
			const auto scaled_max = max / decimal_scale<value_t, fraction_digits_num>::value;			
			const auto test_value = static_cast<value_t>(scaled_max);
			
			REQUIRE(test_value != 0);

			fixed_point_number<value_t, fraction_digits_num> a = test_value;
			fixed_point_number<value_t, fraction_digits_num> b = a;

			REQUIRE_THROWS_AS(a + b, fixed_point_out_of_range_error);
			REQUIRE_THROWS_AS(-b - b, fixed_point_out_of_range_error);
			
			if (scaled_max != 1)
			{
				REQUIRE_THROWS_AS(a * b, fixed_point_out_of_range_error);
			}
		}
	}; // fixed_point_out_of_range_test

	TEMPLATE_LIST_TEST_CASE("Basic arithmetic", "", template_test_types)
	{
		using fixed_point_type = fixed_point_number<TestType, 2>;
		
		const fixed_point_type a = 0.1;
		const fixed_point_type b = 0.2;

		REQUIRE(a + 0 == a);
		REQUIRE(a - 0 == a);
		REQUIRE(0 - a == -a);
		REQUIRE(0 + a == a);

		REQUIRE(a + b == 0.3);
		REQUIRE(b + a == 0.3);
		REQUIRE(a - b == -0.1);
		REQUIRE(b - a == 0.1);

		REQUIRE(-a == -0.1);
		REQUIRE(-b == -0.2);
		REQUIRE(+a == 0.1);
		REQUIRE(+b == 0.2);

		REQUIRE(-(-a) == 0.1);

		REQUIRE(a * 0 == 0);
		REQUIRE(0 * a == 0);
		REQUIRE(a * 1 == a);
		REQUIRE(1 * a == a);
		REQUIRE(a * -1 == -a);
		REQUIRE(-1 * a == -a);

		REQUIRE(b / 1 == 0.2);
		REQUIRE(b / -1 == -0.2);
		REQUIRE(-b / 1 == -0.2);
		REQUIRE(-b / -1 == 0.2);

		REQUIRE(a * a == 0.01);
		REQUIRE(b * b == 0.04);
		REQUIRE(a * b == 0.02);
		REQUIRE(b * a == 0.02);

		REQUIRE(a * -a == -0.01);
		REQUIRE(b * -b == -0.04);
		REQUIRE(a * -b == -0.02);

		REQUIRE(-a * -a == 0.01);
		REQUIRE(-b * -b == 0.04);
		REQUIRE(-a * -b == 0.02);

		REQUIRE(b / b == 1);
		REQUIRE(-b / b == -1);
		REQUIRE(b / -b == -1);
		REQUIRE(-b / -b == 1);

		REQUIRE(a / b == 0.5);
	}

	TEMPLATE_LIST_TEST_CASE("Basic arithmetic assignment operators", "", template_test_types)
	{
		using fixed_point_type = fixed_point_number<TestType, 2>;
		
		fixed_point_type a = 0.1;
		fixed_point_type b = 0.2;

		SECTION("+= operator")
		{
			a += b;
			REQUIRE(a == 0.3);
			REQUIRE(b == 0.2);
		}

		SECTION("-= operator")
		{
			a -= b;
			REQUIRE(a == -0.1);
			REQUIRE(b == 0.2);
		}

		SECTION("*= operator")
		{
			a *= b;
			REQUIRE(a == 0.02);
			REQUIRE(b == 0.2);
		}

		SECTION("/= operator")
		{
			a /= b;
			REQUIRE(a == 0.5);
			REQUIRE(b == 0.2);
		}
	}

	TEMPLATE_LIST_TEST_CASE("Increment and decrement operators", "", template_test_types)
	{
		using fixed_point_type = fixed_point_number<TestType, 2>;
		
		fixed_point_type a = 0.1;

		SECTION("prefix ++ operator")
		{
			const auto b = ++a;
			REQUIRE(a == 1.1);
			REQUIRE(b == a);
		}

		SECTION("postfix ++ operator")
		{
			const auto b = a++;
			REQUIRE(a == 1.1);
			REQUIRE(b == 0.1);
		}

		SECTION("prefix -- operator")
		{
			const auto b = --a;
			REQUIRE(a == -0.9);
			REQUIRE(b == a);
		}

		SECTION("postfix -- operator")
		{
			const auto b = a--;
			REQUIRE(a == -0.9);
			REQUIRE(b == 0.1);
		}
	}

	TEMPLATE_LIST_TEST_CASE("Unary minus overflow", "", template_test_types)
	{
		using fixed_point_type = fixed_point_number<TestType, 0>;
		const fixed_point_type val = std::numeric_limits<TestType>::min();
		
		REQUIRE_NOTHROW(-(val + 1));
		REQUIRE_THROWS_AS(-val, fixed_point_out_of_range_error);
	}

	TEMPLATE_LIST_TEST_CASE("Cast to integer", "", template_test_types)
	{
		using fixed_point_type = fixed_point_number<TestType, 1>;
		const fixed_point_type val = 1;

		const TestType i = static_cast<TestType>(val);
		REQUIRE(i == 1);

		const fixed_point_type val2 = 1.1;
		const TestType i2 = static_cast<TestType>(val2);
		REQUIRE(i2 == 1);
	}

	TEMPLATE_LIST_TEST_CASE("Cast to float", "", template_test_types)
	{
		using fixed_point_type = fixed_point_number<TestType, 1>;
		const fixed_point_type val = 1.1;

		const auto f = static_cast<float>(val);
		const auto d = static_cast<double>(val);
		const auto ld = static_cast<long double>(val);

		REQUIRE(f == Approx(1.1f));
		REQUIRE(d == Approx(1.1));
		REQUIRE(ld == Approx(1.1l));
	}

	TEST_CASE("Max decimal digits num")
	{
		const auto zero_dig_num = max_decimal_digits_num<int, 0>::value;
		const auto one_dig_num = max_decimal_digits_num<int, 1>::value;
		const auto nine_dig_num = max_decimal_digits_num<int, 9>::value;
		const auto ten_dig_num = max_decimal_digits_num<int, 10>::value;
		const auto hundred_dig_num = max_decimal_digits_num<int, 100>::value;
		const auto thousand_dig_num = max_decimal_digits_num<int, 1000>::value;

		REQUIRE(zero_dig_num == 1);
		REQUIRE(one_dig_num == 1);
		REQUIRE(nine_dig_num == 1);
		REQUIRE(ten_dig_num == 2);
		REQUIRE(hundred_dig_num == 3);
		REQUIRE(thousand_dig_num == 4);

		const auto int8_dig_num = max_decimal_digits_num<std::int8_t>::value;
		const auto int16_dig_num = max_decimal_digits_num<std::int16_t>::value;
		const auto int32_dig_num = max_decimal_digits_num<std::int32_t>::value;
		const auto int64_dig_num = max_decimal_digits_num<std::int64_t>::value;

		REQUIRE(int8_dig_num == 3);
		REQUIRE(int16_dig_num == 5);
		REQUIRE(int32_dig_num == 10);
		REQUIRE(int64_dig_num == 19);
	}

	TEST_CASE("Max decimal scale")
	{
		REQUIRE((decimal_scale<long, 0>::value == 1));
		REQUIRE((decimal_scale<long, 1>::value == 10));
		REQUIRE((decimal_scale<long, 2>::value == 100));
		REQUIRE((decimal_scale<long, 3>::value == 1000));
		REQUIRE((decimal_scale<long, 4>::value == 10000));
		REQUIRE((decimal_scale<long, 5>::value == 100000));
		REQUIRE((decimal_scale<long, 6>::value == 1000000));
	}

	TEST_CASE("Test default_round_policy")
	{
		REQUIRE(default_round_policy::round<int>(0.0) == 0);
		REQUIRE(default_round_policy::round<int>(1.0) == 1);
		REQUIRE(default_round_policy::round<int>(-1.0) == -1);
		REQUIRE(default_round_policy::round<int>(0.499) == 0);
		REQUIRE(default_round_policy::round<int>(-0.499) == 0);
		REQUIRE(default_round_policy::round<int>(0.5) == 1);
		REQUIRE(default_round_policy::round<int>(-0.5) == -1);
		REQUIRE(default_round_policy::round<int>(1.4) == 1);
		REQUIRE(default_round_policy::round<int>(-1.4) == -1);
		REQUIRE(default_round_policy::round<int>(1.5) == 2);
		REQUIRE(default_round_policy::round<int>(-1.5) == -2);
		REQUIRE(default_round_policy::round<int>(2.1) == 2);
		REQUIRE(default_round_policy::round<int>(-2.1) == -2);

		REQUIRE(default_round_policy::round_div<int>(0, 1) == 0);
		REQUIRE(default_round_policy::round_div<int>(0, -1) == 0);

		REQUIRE(default_round_policy::round_div<int>(1, 1) == 1);
		REQUIRE(default_round_policy::round_div<int>(1, -1) == -1);
		REQUIRE(default_round_policy::round_div<int>(-1, 1) == -1);
		REQUIRE(default_round_policy::round_div<int>(1, -1) == -1);

		REQUIRE(default_round_policy::round_div<int>(10, 10) == 1);
		REQUIRE(default_round_policy::round_div<int>(-10, 10) == -1);
		REQUIRE(default_round_policy::round_div<int>(10, -10) == -1);
		REQUIRE(default_round_policy::round_div<int>(-10, -10) == 1);

		REQUIRE(default_round_policy::round_div<int>(1, 10) == 0);
		REQUIRE(default_round_policy::round_div<int>(-1, 10) == 0);
		REQUIRE(default_round_policy::round_div<int>(1, -10) == 0);
		REQUIRE(default_round_policy::round_div<int>(-1, -10) == 0);

		REQUIRE(default_round_policy::round_div<int>(11111, 10) == 1111);
		REQUIRE(default_round_policy::round_div<int>(11114, 10) == 1111);
		REQUIRE(default_round_policy::round_div<int>(11115, 10) == 1112);
		REQUIRE(default_round_policy::round_div<int>(11119, 10) == 1112);

		REQUIRE(default_round_policy::round_div<int>(-11111, 10) == -1111);
		REQUIRE(default_round_policy::round_div<int>(-11114, 10) == -1111);
		REQUIRE(default_round_policy::round_div<int>(-11115, 10) == -1112);
		REQUIRE(default_round_policy::round_div<int>(-11119, 10) == -1112);
	}

	TEST_CASE("Construct")
	{			
		list_of_types_to_test::for_each_type(fixed_point_tester<fixed_point_construction_test>());
	}

	TEST_CASE("Assign")
	{
		list_of_types_to_test::for_each_type(fixed_point_tester<fixed_point_assign_test>());
	}

	TEST_CASE("Add")
	{
		list_of_types_to_test::for_each_type(fixed_point_tester<fixed_point_add_test>());
	}

	TEST_CASE("Subtract")
	{
		list_of_types_to_test::for_each_type(fixed_point_tester<fixed_point_subtract_test>());
	}

	TEST_CASE("Multiply divide")
	{
		list_of_types_to_test::for_each_type(fixed_point_tester<fixed_point_multiply_divide_test>());
	}

	TEMPLATE_LIST_TEST_CASE("Comparison operators", "", template_test_types)
	{
		using fixed_point_t = fixed_point_number<TestType, 2>;
		
		fixed_point_t a = 0.57;
		fixed_point_t b = a;
		fixed_point_t c = a * 1.11;
		fixed_point_t d = a / 1.11;

		REQUIRE(a == a);
		REQUIRE(a == b);
		REQUIRE(b == a);
		REQUIRE(!(b == c));
		REQUIRE(!(c == b));
		REQUIRE(!(d == c));
		REQUIRE(!(c == d));

		REQUIRE(!(a != a));
		REQUIRE(!(a != b));
		REQUIRE(!(b != a));
		REQUIRE(b != c);
		REQUIRE(c != b);
		REQUIRE(d != c);
		REQUIRE(c != d);

		REQUIRE(!(a < a));
		REQUIRE(!(a < b));
		REQUIRE(!(b < a));
		REQUIRE(!(c < b));
		REQUIRE(!(a < d));
		REQUIRE(a < c);
		REQUIRE(d < a);
		REQUIRE(d < c);

		REQUIRE(a <= a);
		REQUIRE(a <= b);
		REQUIRE(b <= a);
		REQUIRE(!(c <= b));
		REQUIRE(!(a <= d));
		REQUIRE(a <= c);
		REQUIRE(d <= a);
		REQUIRE(d <= c);

		REQUIRE(!(a > a));
		REQUIRE(!(a > b));
		REQUIRE(!(b > a));
		REQUIRE(c > b);
		REQUIRE(a > d);
		REQUIRE(!(a > c));
		REQUIRE(!(d > a));
		REQUIRE(!(d > c));

		REQUIRE(a >= a);
		REQUIRE(a >= b);
		REQUIRE(b >= a);
		REQUIRE(c >= b);
		REQUIRE(a >= d);
		REQUIRE(!(a >= c));
		REQUIRE(!(d >= a));
		REQUIRE(!(d >= c));
	}
	
	TEST_CASE("Fixed point number out of range")
	{
		list_of_types_to_test::for_each_type(fixed_point_tester<fixed_point_out_of_range_test>());
	}

	TEST_CASE("Convert to string")
	{
		// test simplified version of to_string and to_wstring			
		{
			fixed_point_number<std::int64_t, 4> a = 1234.9876;
			REQUIRE(fixed_point_arithmetic::to_string(a) == "1234.9876");
			REQUIRE((fixed_point_arithmetic::to_wstring(a) == L"1234.9876"));
		}

		{
			fixed_point_number<std::int64_t, 4> a = 1234.0;
			REQUIRE(fixed_point_arithmetic::to_string(a) == "1234.0000");
			REQUIRE((fixed_point_arithmetic::to_wstring(a) == L"1234.0000"));
		}

		{
			fixed_point_number<std::int64_t, 4> a = 0.1234;
			REQUIRE(fixed_point_arithmetic::to_string(a) == "0.1234");
			REQUIRE((fixed_point_arithmetic::to_wstring(a) == L"0.1234"));
		}

		{
			fixed_point_number<std::int64_t, 4> a = -1234.9876;
			REQUIRE(fixed_point_arithmetic::to_string(a) == "-1234.9876");
			REQUIRE((fixed_point_arithmetic::to_wstring(a) == L"-1234.9876"));
		}

		{
			fixed_point_number<std::int64_t, 4> a = -0.1234;
			REQUIRE(fixed_point_arithmetic::to_string(a) == "-0.1234");
			REQUIRE((fixed_point_arithmetic::to_wstring(a) == L"-0.1234"));
		}

		{
			fixed_point_number<std::int64_t, 4> a = 1234.0076;
			REQUIRE(fixed_point_arithmetic::to_string(a) == "1234.0076");
			REQUIRE((fixed_point_arithmetic::to_wstring(a) == L"1234.0076"));
		}

		{
			fixed_point_number<std::int64_t, 4> a = 1234.007;
			REQUIRE(fixed_point_arithmetic::to_string(a) == "1234.0070");
			REQUIRE((fixed_point_arithmetic::to_wstring(a) == L"1234.0070"));
		}

		{
			fixed_point_number<std::int64_t, 4> a = -1234.0076;
			REQUIRE(fixed_point_arithmetic::to_string(a) == "-1234.0076");
			REQUIRE((fixed_point_arithmetic::to_wstring(a) == L"-1234.0076"));
		}

		{
			fixed_point_number<std::int64_t, 4> a = -1234.007;
			REQUIRE(fixed_point_arithmetic::to_string(a) == "-1234.0070");
			REQUIRE((fixed_point_arithmetic::to_wstring(a) == L"-1234.0070"));
		}
	}
}