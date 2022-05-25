// Copyright (c) 2022 Ilia Funtov
// Distributed under the Boost Software License, Version 1.0. (http://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <cfenv>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

namespace fixed_point_arithmetic
{
	namespace details
	{
		template <typename value_t>
		constexpr int calc_max_decimal_digits_num(value_t value)
		{
			if ((value / 10) == 0)
				return 1;
			else
				return calc_max_decimal_digits_num(value / 10) + 1;
		}

		template <typename T>
		T gcd(T a, T b)
		{
			while (a != 0)
			{
				const auto c = a;
				a = b % a;
				b = c;
			}
			return b;
		}

		template <typename T>
		bool is_add_overflow(T a, T b, T result) // check if a + b is out of T range
		{
			if (a > 0 && b > 0)
				return result < 0;
			else if (a < 0 && b < 0)
				return result > 0;

			return false;
		}

		template <typename T>
		bool is_subtract_overflow(T a, T b, T result) // check if a - b is out of T range
		{
			return is_add_overflow(b, result, a); // result = a - b, then a = b + result
		}

		template <typename T>
		struct next_storage_type {};

		template <typename T>
		struct next_storage_type<const T> : next_storage_type<T> {};

		template <>
		struct next_storage_type<signed char> { using type = short; };

		template <>
		struct next_storage_type<short> { using type = long; };

		template <>
		struct next_storage_type<int> { using type = long long; };

		template <>
		struct next_storage_type<long> { using type = long long; };

		template <>
		struct next_storage_type<long long> { using type = long long; };
	}

	template <typename value_t, unsigned int digit_num>
	struct decimal_scale;

	template <typename value_t>
	struct decimal_scale<value_t, 0> : std::integral_constant<value_t, 1>
	{
	};

	template <typename value_t, unsigned int digit_num>
	struct decimal_scale : std::integral_constant<value_t, decimal_scale<value_t, digit_num - 1>::value * 10>
	{
	};

	template <typename value_t, value_t value = std::numeric_limits<value_t>::max()>
	struct max_decimal_digits_num: std::integral_constant<value_t, details::calc_max_decimal_digits_num(value)>
	{
		static_assert(std::is_integral<value_t>::value, "max_decimal_digits_num: only integral types are supported.");
	};

	class fixed_point_out_of_range_error : public std::range_error
	{
	public:
		explicit fixed_point_out_of_range_error(const std::string & message) : std::range_error(message) {}
		explicit fixed_point_out_of_range_error(const char * message) : std::range_error(message) {}
	};

	class fixed_point_conversion_error : public std::range_error
	{
	public:
		explicit fixed_point_conversion_error(const std::string & message) : std::range_error(message) {}
		explicit fixed_point_conversion_error(const char * message) : std::range_error(message) {}
	};

	class round_policy_error : public std::range_error
	{
	public:
		round_policy_error() : std::range_error("Round value is out of range of specified destination type.") {}
	};

	class default_round_policy
	{
	public:
		template <typename value_to_t, typename value_from_t>
		static value_to_t round(value_from_t value)
		{
			static_assert(std::is_floating_point<value_from_t>::value, "round operation is applicable only for floating point numbers.");
			const auto rounded = std::round(value);
			const auto casted = static_cast<value_to_t>(rounded);
			const auto casted_to_source = static_cast<value_from_t>(casted);
			if (casted_to_source != rounded)
			{
				throw round_policy_error();
			}
			return casted;
		}

		template <typename value_t>
		static value_t round_div(value_t value, value_t divisor)
		{
			static_assert(std::is_integral<value_t>::value, "round_div operation is applicable only for integer numbers.");

			if (divisor == 0 )
			{
				throw std::invalid_argument("divisor cannot be zero.");
			}

			if (value == 0)
				return 0;

			const auto abs_divisor = std::abs(divisor);
			auto divided_value = value / abs_divisor;
			const auto remainder = std::abs(value) % abs_divisor;
			if (remainder * 2 >= abs_divisor)
			{
				divided_value += (value < 0) ? -1 : 1;
			}

			return static_cast<value_t>((divisor < 0) ? -divided_value : divided_value);
		}
	};

	template <
		typename value_t,
		unsigned int fraction_digits_num,
		typename round_policy_t = default_round_policy>
	class fixed_point_number
	{
	public:
		static_assert(std::is_integral<value_t>::value, "Value type must be integral.");
		static_assert(std::is_signed<value_t>::value, "Value type must be a signed type.");

		using value_type = value_t;
		using round_policy_type = round_policy_t;

		struct number_parts
		{
			bool negative;
			value_type integer;
			value_type fractional;
		};

		constexpr static auto scale_value = decimal_scale<value_type, fraction_digits_num>::value;
		static_assert(scale_value != 0, "Scale value must not be zero.");

		fixed_point_number() : _value() {}

		fixed_point_number(const fixed_point_number & src) : _value(src._value) {}

		template <typename source_t>
		fixed_point_number(const source_t & src) : _value(convert_from_source(src)) {}

		fixed_point_number & operator = (const fixed_point_number & src)
		{
			_value = src._value;
			return *this;
		}

		template <typename destination_type>
		explicit operator destination_type () const
		{
			return convert_to_destination<destination_type>(_value);
		}

		number_parts get_parts() const
		{
			const auto int_part = _value / scale_value;
			const auto int_part_scaled = int_part * scale_value;
			const auto fractional_part = (_value > int_part_scaled) ? _value - int_part_scaled : int_part_scaled - _value;

			bool negative = _value < 0;
			return number_parts{ negative , static_cast<value_type>(negative ? -int_part: int_part), static_cast<value_type>(fractional_part) };
		}

		fixed_point_number operator + () const
		{
			return *this;
		}

		fixed_point_number operator - () const
		{
			fixed_point_number result;
			result._value = -_value;
			
			if ((result._value > 0 && _value > 0) || (result._value < 0 && _value < 0))
			{
				throw fixed_point_out_of_range_error("Result of unary minus operation is out of range.");
			}

			return result;
		}

		fixed_point_number & operator += (const fixed_point_number & x)
		{
			auto tmp_value = _value;
			tmp_value += x._value;

			if (details::is_add_overflow(_value, x._value, tmp_value))
			{
				throw fixed_point_out_of_range_error("Result of add operation is out of range.");
			}

			_value = tmp_value;
			
			return *this;
		}

		fixed_point_number & operator -= (const fixed_point_number & x)
		{
			auto tmp_value = _value;
			tmp_value -= x._value;

			if (details::is_subtract_overflow(_value, x._value, tmp_value))
			{
				throw fixed_point_out_of_range_error("Result of subtract operation is out of range.");
			}

			_value = tmp_value;

			return *this;
		}

		fixed_point_number & operator /= (const fixed_point_number & x)
		{
			_value = mult_div(_value, scale_value, x._value);
			return *this;
		}

		fixed_point_number & operator *= (const fixed_point_number & x)
		{
			_value = mult_div(_value, x._value, scale_value);
			return *this;
		}

		fixed_point_number & operator %= (const fixed_point_number & x) = delete;
		
		fixed_point_number & operator ++ () // prefix increment
		{
			auto tmp_value = _value;
			tmp_value += scale_value;

			if (details::is_add_overflow(_value, scale_value, tmp_value))
			{
				throw fixed_point_out_of_range_error("Result of increment operation is out of range.");
			}

			_value = tmp_value;

			return *this;
		}

		fixed_point_number operator ++ (int) // postfix increment
		{
			const auto prev_value = *this;
			++(*this);
			return prev_value;
		}

		fixed_point_number & operator -- () // prefix decrement
		{
			auto tmp_value = _value;
			tmp_value -= scale_value;

			if (details::is_subtract_overflow(_value, scale_value, tmp_value))
			{
				throw fixed_point_out_of_range_error("Result of decrement operation is out of range.");
			}

			_value = tmp_value;

			return *this;
		}

		fixed_point_number operator -- (int) // postfix decrement
		{
			const auto prev_value = *this;
			--(*this);
			return prev_value;
		}

		friend fixed_point_number operator + (fixed_point_number lhs, const fixed_point_number & rhs)
		{
			lhs += rhs;
			return lhs;
		}

		friend fixed_point_number operator - (fixed_point_number lhs, const fixed_point_number & rhs)
		{
			lhs -= rhs;
			return lhs;
		}

		friend fixed_point_number operator * (fixed_point_number lhs, const fixed_point_number & rhs)
		{
			lhs *= rhs;
			return lhs;
		}

		friend fixed_point_number operator / (fixed_point_number lhs, const fixed_point_number & rhs)
		{
			lhs /= rhs;
			return lhs;
		}

		friend bool operator == (const fixed_point_number & lhs, const fixed_point_number & rhs)
		{
			return lhs._value == rhs._value;
		}

		friend bool operator != (const fixed_point_number & lhs, const fixed_point_number & rhs)
		{
			return lhs._value != rhs._value;
		}

		friend bool operator < (const fixed_point_number & lhs, const fixed_point_number & rhs)
		{
			return lhs._value < rhs._value;
		}

		friend bool operator > (const fixed_point_number & lhs, const fixed_point_number & rhs)
		{
			return lhs._value > rhs._value;
		}

		friend bool operator <= (const fixed_point_number & lhs, const fixed_point_number & rhs)
		{
			return lhs._value <= rhs._value;
		}

		friend bool operator >= (const fixed_point_number & lhs, const fixed_point_number & rhs)
		{
			return lhs._value >= rhs._value;
		}

		friend void swap(fixed_point_number & lhs, fixed_point_number & rhs)
		{
			std::swap(lhs._value, rhs._value);
		}

	private:
		template <typename destination_t, typename source_t>
		static
		destination_t range_checked_cast(source_t src)
		{
			if (src < std::numeric_limits<destination_t>::min() || src > std::numeric_limits<destination_t>::max())
			{
				throw fixed_point_out_of_range_error("Result of operation is out of range.");
			}

			return static_cast<destination_t>(src);
		}

		template <typename source_t>
		static
		typename std::enable_if<std::is_integral<source_t>::value, value_type>::type convert_from_source(const source_t & src)
		{
			const auto value = src * scale_value;
			if ((value / scale_value) != src || 
				value < std::numeric_limits<value_type>::min() || value > std::numeric_limits<value_type>::max())
			{
				throw fixed_point_conversion_error("Result of conversion from integer number does not fit into the storage type.");
			}

			return static_cast<value_type>(value);
		}

		template <typename destination_t>
		static
		typename std::enable_if<std::is_integral<destination_t>::value, destination_t>::type convert_to_destination(const value_type & value)
		{
			const auto scaled_value = round_policy_type::round_div(value, scale_value);
			if (scaled_value < std::numeric_limits<destination_t>::min() || scaled_value > std::numeric_limits<destination_t>::max())
			{
				throw fixed_point_conversion_error("Integral destination type cannot fit value from fixed point number.");
			}

			return static_cast<destination_t>(scaled_value);
		}

		template <typename source_t>
		static
		typename std::enable_if<std::is_floating_point<source_t>::value, value_type>::type convert_from_source(const source_t & src)
		{
			std::feclearexcept(FE_OVERFLOW);
			const auto value = static_cast<conversion_float_type>(src) * scale_value;
			if (std::fetestexcept(FE_OVERFLOW))
			{
				throw fixed_point_conversion_error("Conversion from floating point number caused overflow.");
			}

			return round_policy_type::template round<value_type>(value);
		}

		template <typename destination_t>
		static
		typename std::enable_if<std::is_floating_point<destination_t>::value, destination_t>::type convert_to_destination(const value_type & value)
		{
			std::feclearexcept(FE_UNDERFLOW);
			const auto scaled_value = static_cast<destination_t>(static_cast<conversion_float_type>(value) / scale_value);
			if (std::fetestexcept(FE_UNDERFLOW))
			{
				throw fixed_point_conversion_error("Conversion to floating point number caused underflow.");
			}

			return scaled_value;
		}

		template <typename T>
		static T mult_div(T value1, T value2, T divisor)
		{
			if (divisor == 0)
			{
				throw std::invalid_argument("Divisor cannot be zero.");
			}

			if (value1 != 0)
			{
				const auto common_divisor = details::gcd(std::abs(value1), std::abs(divisor));
				if (common_divisor != 1)
				{
					value1 = static_cast<T>(value1 / common_divisor);
					divisor = static_cast<T>(divisor / common_divisor);
				}
			}
			else return 0;

			if (value2 != 0)
			{
				const auto common_divisor = details::gcd(std::abs(value2), std::abs(divisor));
				if (common_divisor != 1)
				{
					value2 = static_cast<T>(value2 / common_divisor);
					divisor = static_cast<T>(divisor / common_divisor);
				}
			}
			else return 0;

			const auto mult_result = static_cast<T>(value1 * value2);
			if (mult_result / value1 != value2)
			{
				using mult_type_t = decltype(mult_result);
				using mult_next_storage_t = typename details::next_storage_type<mult_type_t>::type;
				
				const auto mult_result_extended = mult_overflow_handler<mult_type_t, mult_next_storage_t>::handle(value1, value2);
				using result_common_t = typename std::common_type<decltype(mult_result_extended), decltype(divisor)>::type;
				return range_checked_cast<T>(round_policy_type::round_div(static_cast<result_common_t>(mult_result_extended), static_cast<result_common_t>(divisor)));
			}

			using result_common_t = typename std::common_type<decltype(mult_result), decltype(divisor)>::type;
			return range_checked_cast<T>(round_policy_type::round_div(static_cast<result_common_t>(mult_result), static_cast<result_common_t>(divisor)));
		}

		template <typename mult_type, typename mult_type_extended>
		struct mult_overflow_handler_impl;

		template <typename mult_type, typename mult_type_extended>
		struct mult_overflow_handler : mult_overflow_handler_impl<typename std::decay<mult_type>::type, typename std::decay<mult_type_extended>::type>
		{
		};

		template <typename mult_type, typename mult_type_extended>
		struct mult_overflow_handler_impl
		{
			static mult_type_extended handle(mult_type value1, mult_type value2)
			{
				const auto result = static_cast<mult_type_extended>(value1) * static_cast<mult_type_extended>(value2);
				if (result / value1 != value2)
				{
					throw fixed_point_out_of_range_error("Result of multiply/division operation is out of range.");
				}
				return result;
			}
		};

		template <typename mult_type>
		struct mult_overflow_handler_impl<mult_type, mult_type>
		{
			static mult_type handle(mult_type, mult_type)
			{
				throw fixed_point_out_of_range_error("Result of multiply/division operation is out of range.");
			}
		};

		using conversion_float_type = long double;

		value_type _value;
	};

	template <typename value_t, unsigned int fraction_digits_num, typename round_policy_t>
	std::string to_string(const fixed_point_number<value_t, fraction_digits_num, round_policy_t> & value)
	{
		// simplified version without locale specific formatting
		const auto parts = value.get_parts();
		std::string sign;
		if (parts.negative) sign += '-';
		const auto fractional = std::to_string(parts.fractional);
		const auto fract_padding = (fraction_digits_num > fractional.size()) ? std::string(fraction_digits_num - fractional.size(), '0') : std::string();
		return sign + std::to_string(parts.integer) + '.' + fract_padding + fractional;
	}

	template <typename value_t, unsigned int fraction_digits_num, typename round_policy_t>
	std::wstring to_wstring(const fixed_point_number<value_t, fraction_digits_num, round_policy_t> & value)
	{
		// simplified version without locale specific formatting
		const auto parts = value.get_parts();
		std::wstring sign;
		if (parts.negative) sign += L'-';
		const auto fractional = std::to_wstring(parts.fractional);
		const auto fract_padding = (fraction_digits_num > fractional.size()) ? std::wstring(fraction_digits_num - fractional.size(), L'0') : std::wstring();
		return sign + std::to_wstring(parts.integer) + L'.' + fract_padding + fractional;
	}

	template <typename value_t, unsigned int fraction_digits_num, typename round_policy_t>
	std::ostream & operator << (std::ostream & os, const fixed_point_number<value_t, fraction_digits_num, round_policy_t> & value)
	{
		os << to_string(value);
		return os;
	}

}
