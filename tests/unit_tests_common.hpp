// Copyright (c) 2022 Ilia Funtov
// Distributed under the Boost Software License, Version 1.0. (http://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <utility>

namespace unit_tests_common
{
	template<class... types>
	class type_list;

	template<typename this_type, typename... rest_types>
	class type_list<this_type, rest_types...> : private type_list<rest_types...>
	{
	public:
		template <typename handler_t>
		static void for_each_type(handler_t && handler)
		{
			handler.template operator()<this_type>();
			parent_type::for_each_type(std::forward<handler_t>(handler));
		}
	private:
		using parent_type = type_list<rest_types...>;
	};

	template<>
	class type_list<>
	{
	public:
		template <typename handler_t>
		static void for_each_type(handler_t &&) {}
	};
}
