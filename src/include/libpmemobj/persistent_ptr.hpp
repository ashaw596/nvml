/*
 * Copyright 2015-2016, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *
 *     * Neither the name of the copyright holder nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * persistent_ptr.hpp -- persistent smart pointer
 */

#ifndef PMEMOBJ_PERSISTENT_PTR_HPP
#define PMEMOBJ_PERSISTENT_PTR_HPP

#include <memory>
#include <cassert>
#include <ostream>

#include "libpmemobj.h"
#include "libpmemobj/detail/specialization.hpp"
#include "libpmemobj/detail/common.hpp"

namespace nvml
{

namespace obj
{
	/*
	 * Persistent pointer class.
	 *
	 * persistent_ptr implements a smart ptr. It encapsulates the PMEMoid
	 * fat pointer and provides member access, dereference and array
	 * access operators.
	 * This type does NOT manage the life-cycle of the object.
	 */
	template<typename T>
	class persistent_ptr
	{
		template<typename Y>
		friend class persistent_ptr;

		typedef persistent_ptr<T> this_type;
	public:
		/* used for easy underlying type access */
		typedef typename nvml::detail::sp_element<T>::type element_type;

		/*
		 * Default constructor is NULL-initialized.
		 */
		persistent_ptr() : oid(OID_NULL)
		{
			verify_type();
		}

		/*
		 * Curly braces initialization is not used because the
		 * PMEMoid is a plain C (POD) type and we can't add a default
		 * constructor in there.
		 */

		/*
		 *  Default null constructor, zeroes the PMEMoid.
		 */
		persistent_ptr(std::nullptr_t) noexcept : oid(OID_NULL)
		{
			verify_type();
		}

		/*
		 * PMEMoid constructor.
		 */
		persistent_ptr(PMEMoid o) noexcept : oid(o)
		{
			verify_type();
		}

		/*
		 * Converting constructor from a different persistent_ptr<>.
		 */
		template<typename Y, typename = typename
		std::enable_if<std::is_convertible<Y*, T*>::value>::type>
		persistent_ptr(const persistent_ptr<Y> &r) noexcept : oid(r.oid)
		{
			verify_type();
		}

		/*
		 * Copy constructor.
		 */
		persistent_ptr(const persistent_ptr &r) noexcept : oid(r.oid)
		{
			verify_type();
		}

		/*
		 * Defaulted move constructor.
		 */
		persistent_ptr(persistent_ptr &&r) noexcept = default;

		/*
		 * Defaulted move assignment operator.
		 */
		persistent_ptr &
		operator=(persistent_ptr &&r) noexcept = default;

		/*
		 * Assignment operator.
		 */
		persistent_ptr &
		operator=(const persistent_ptr &r) noexcept
		{
			detail::conditional_add_to_tx(this);
			this_type(r).swap(*this);

			return *this;
		}

		/*
		 * Converting assignment operator from a different
		 * persistent_ptr<>.
		 */
		template<typename Y, typename = typename
		std::enable_if<std::is_convertible<Y*, T*>::value>::type>
		persistent_ptr &
		operator=(const persistent_ptr<Y> &r) noexcept
		{
			detail::conditional_add_to_tx(this);
			this_type(r).swap(*this);

			return *this;
		}

		/*
		 * Dereference operator.
		 */
		typename nvml::detail::sp_dereference<T>::type
			operator*() const noexcept
		{
			return *get();
		}

		/*
		 * Member access operator.
		 */
		typename nvml::detail::sp_member_access<T>::type
			operator->() const noexcept
		{
			return get();
		}

		/*
		 * Array access operator.
		 */
		typename nvml::detail::sp_array_access<T>::type
			operator[](std::ptrdiff_t i) const noexcept
		{
			assert(i >= 0 &&
				(i < nvml::detail::sp_extent<T>::value ||
				nvml::detail::sp_extent<T>::value == 0) &&
				"persistent array index out of bounds");

			return get()[i];
		}

		/*
		 * Get a direct pointer.
		 *
		 * Calculates and returns a direct pointer to the object.
		 */
		element_type *
		get() const noexcept
		{
			return (element_type *)pmemobj_direct(this->oid);
		}

		/*
		 * Swaps two persistent_ptr objects of the same type.
		 */
		void
		swap(persistent_ptr &other) noexcept
		{
			std::swap(this->oid, other.oid);
		}

		/* Unspecified bool type. */
		typedef element_type *
			(persistent_ptr<T>::*unspecified_bool_type)() const;

		/*
		 * Unspecified bool type conversion operator.
		 */
		operator unspecified_bool_type() const noexcept
		{
			return OID_IS_NULL(this->oid) ? 0 :
					&persistent_ptr<T>::get;
		}

		/*
		 * Bool conversion operator.
		 */
		explicit operator bool() const noexcept
		{
			return get() != nullptr;
		}

		/*
		 * Get PMEMoid encapsulated by this object.
		 *
		 * For C API compatibility.
		 */
		const PMEMoid &
		raw() const noexcept
		{
			return this->oid;
		}

		/*
		 * Get pointer to PMEMoid encapsulated by this object.
		 *
		 * For C API compatibility.
		 */
		PMEMoid *
		raw_ptr() noexcept
		{
			return &(this->oid);
		}

		/*
		 * Prefix increment operator.
		 */
		inline persistent_ptr<T> &operator++()
		{
			detail::conditional_add_to_tx(this);
			this->oid.off += sizeof(T);

			return *this;
		}

		/*
		 * Postfix increment operator.
		 */
		inline persistent_ptr<T> operator++(int)
		{
			PMEMoid noid = this->oid;
			++(*this);

			return persistent_ptr<T>(noid);
		}

		/*
		 * Prefix decrement operator.
		 */
		inline persistent_ptr<T> &operator--()
		{
			detail::conditional_add_to_tx(this);
			this->oid.off -= sizeof(T);

			return *this;
		}

		/*
		 * Postfix decrement operator.
		 */
		inline persistent_ptr<T> operator--(int)
		{
			PMEMoid noid = this->oid;
			--(*this);

			return persistent_ptr<T>(noid);
		}

		/*
		 * Addition assignment operator.
		 */
		inline persistent_ptr<T> &operator+=(std::ptrdiff_t s)
		{
			detail::conditional_add_to_tx(this);
			this->oid.off += s * sizeof(T);

			return *this;
		}

		/*
		 * Subtraction assignment operator.
		 */
		inline persistent_ptr<T> &operator-=(std::ptrdiff_t s)
		{
			detail::conditional_add_to_tx(this);
			this->oid.off -= s * sizeof(T);

			return *this;
		}
	private:
		/* The underlying PMEMoid of the held object. */
		PMEMoid oid;

		/*
		 * C++ persistent memory support has following type limitations:
		 * en.cppreference.com/w/cpp/types/is_polymorphic
		 * en.cppreference.com/w/cpp/types/is_default_constructible
		 * en.cppreference.com/w/cpp/types/is_destructible
		 */
		void
		verify_type()
		{
			static_assert(!std::is_polymorphic
				<element_type>::value,
			"Polymorphic types are not supported");
		}
	};

	/*
	 * Swaps two persistent_ptr objects of the same type.
	 *
	 * Non-member swap function as required by Swappable concept.
	 * en.cppreference.com/w/cpp/concept/Swappable
	 */
	template<class T> inline void
	swap(persistent_ptr<T> & a, persistent_ptr<T> & b) noexcept
	{
		a.swap(b);
	}

	/*
	 * Equality operator.
	 *
	 * This checks if underlying PMEMoids are equal.
	 */
	template<typename T, typename Y> inline bool
	operator==(const persistent_ptr<T> &lhs,
			const persistent_ptr<Y> &rhs) noexcept
	{
		return OID_EQUALS(lhs.raw(), rhs.raw());
	}

	/*
	 * Inequality operator.
	 */
	template<typename T, typename Y> inline bool
	operator!=(const persistent_ptr<T> &lhs,
			const persistent_ptr<Y> &rhs) noexcept
	{
		return !(lhs == rhs);
	}

	/*
	 * Less than operator.
	 *
	 * Returns true if the uuid_lo of lhs is less than the uuid_lo of rhs,
	 * should they be equal, the offsets are compared. Returns false
	 * otherwise.
	 */
	template<typename T, typename Y> inline bool
	operator<(const persistent_ptr<T> &lhs,
			const persistent_ptr<Y> &rhs) noexcept
	{
		if (lhs.raw().pool_uuid_lo == rhs.raw().pool_uuid_lo)
			return lhs.raw().off < rhs.raw().off;
		else
			return lhs.raw().pool_uuid_lo < rhs.raw().pool_uuid_lo;
	}

	/*
	 * Less or equal than operator.
	 *
	 * See less than operator for comparison rules.
	 */
	template<typename T, typename Y> inline bool
	operator<=(const persistent_ptr<T> &lhs,
			const persistent_ptr<Y> &rhs) noexcept
	{
		return !(rhs < lhs);
	}

	/*
	 * Greater than operator.
	 *
	 * See less than operator for comparison rules.
	 */
	template<typename T, typename Y> inline bool
	operator>(const persistent_ptr<T> &lhs,
			const persistent_ptr<Y> &rhs) noexcept
	{
		return (rhs < lhs);
	}

	/*
	 * Greater or equal than operator.
	 *
	 * See less than operator for comparison rules.
	 */
	template<typename T, typename Y> inline bool
	operator>=(const persistent_ptr<T> &lhs,
			const persistent_ptr<Y> &rhs) noexcept
	{
		return !(lhs < rhs);
	}

	/* nullptr comparisons */

	/*
	 * Compare a persistent_ptr with a null pointer.
	 */
	template<typename T> inline bool
	operator<(const persistent_ptr<T> &lhs, std::nullptr_t) noexcept
	{
		return std::less<T*>()(lhs.get(), nullptr);
	}

	/*
	 * Compare a persistent_ptr with a null pointer.
	 */
	template<typename T> inline bool
	operator<(std::nullptr_t, const persistent_ptr<T> &rhs) noexcept
	{
		return std::less<T*>()(nullptr, rhs.get());
	}

	/*
	 * Compare a persistent_ptr with a null pointer.
	 */
	template<typename T> inline bool
	operator<=(const persistent_ptr<T>& lhs, std::nullptr_t) noexcept
	{
		return !(nullptr < lhs);
	}

	/*
	 * Compare a persistent_ptr with a null pointer.
	 */
	template<typename T> inline bool
	operator<=(std::nullptr_t, const persistent_ptr<T>& rhs) noexcept
	{
		return !(rhs < nullptr);
	}

	/*
	 * Compare a persistent_ptr with a null pointer.
	 */
	template<typename T> inline bool
	operator>(const persistent_ptr<T>& lhs, std::nullptr_t) noexcept
	{
		return nullptr < lhs;
	}

	/*
	 * Compare a persistent_ptr with a null pointer.
	 */
	template<typename T> inline bool
	operator>(std::nullptr_t, const persistent_ptr<T>& rhs) noexcept
	{
		return rhs < nullptr;
	}

	/*
	 * Compare a persistent_ptr with a null pointer.
	 */
	template<typename T> inline bool
	operator>=(const persistent_ptr<T>& lhs, std::nullptr_t) noexcept
	{
		return !(lhs < nullptr);
	}

	/*
	 * Compare a persistent_ptr with a null pointer.
	 */
	template<typename T> inline bool
	operator>=(std::nullptr_t, const persistent_ptr<T>& rhs) noexcept
	{
		return !(nullptr < rhs);
	}

	/*
	 * Addition operator for persistent pointers.
	 */
	template<typename T>
	inline persistent_ptr<T> operator+(const persistent_ptr<T> &lhs,
			std::size_t s)
	{
		PMEMoid noid;
		noid.pool_uuid_lo = lhs.raw().pool_uuid_lo;
		noid.off = lhs.raw().off + (s * sizeof(T));
		return persistent_ptr<T>(noid);
	}

	/*
	 * Subtraction operator for persistent pointers.
	 */
	template<typename T>
	inline persistent_ptr<T> operator-(const persistent_ptr<T> &lhs,
			std::size_t s)
	{
		PMEMoid noid;
		noid.pool_uuid_lo = lhs.raw().pool_uuid_lo;
		noid.off = lhs.raw().off - (s * sizeof(T));
		return persistent_ptr<T>(noid);
	}

	/*
	 * Subtraction operator for persistent pointers of identical type.
	 *
	 * Calculates the offset difference of PMEMoids in terms of represented
	 * objects. Calculating the difference of pointers from objects of
	 * different pools is not allowed.
	 */
	template<typename T>
	inline ptrdiff_t operator-(const persistent_ptr<T> &lhs,
			const persistent_ptr<T> &rhs)
	{
		assert(lhs.raw().pool_uuid_lo == rhs.raw().pool_uuid_lo);
		ptrdiff_t d = lhs.raw().off - rhs.raw().off;

		return d / sizeof (T);
	}

	/*
	 * Ostream operator for the persistent pointer.
	 */
	template<typename T> std::ostream &
	operator<<(std::ostream &os, const persistent_ptr<T> &pptr)
	{
		PMEMoid raw_oid = pptr.raw();
		os << std::hex << "0x" << raw_oid.pool_uuid_lo
				<< ", 0x" << raw_oid.off << std::dec;
		return os;
	}

} /* namespace obj */

} /* namespace nvml */

#endif /* PMEMOBJ_PERSISTENT_PTR_HPP */
