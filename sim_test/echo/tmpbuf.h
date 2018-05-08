/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/

#pragma once

#include <assert.h>
#include <stdlib.h>

/*
从 vc7 <atlaloc.h> 里面拷贝出来的，增加了一个 data() 返回 buffer 指针
*/

class CRTAllocator
{
public:
	static void* Reallocate(void* p, size_t nBytes) throw()
	{
		return realloc(p, nBytes);
	}

	static void* Allocate(size_t nBytes) throw()
	{
		return malloc(nBytes);
	}

	static void Free(void* p) throw()
	{
		free(p);
	}
};

template< typename T, int t_nFixedBytes = 128, class Allocator = CRTAllocator >
class TempBuffer
{
public:
	TempBuffer() throw() :
		m_p(NULL)
	{
	}
	TempBuffer(size_t nElements)
		: m_p(NULL)
	{
		Allocate(nElements);
	}

	~TempBuffer() throw()
	{
		if (m_p != reinterpret_cast< T* >(m_abFixedBuffer))
		{
			FreeHeap();
		}
	}

	T* data() throw() // never return NULL
	{
		if (NULL != m_p)
			return m_p;
		return reinterpret_cast< T* >(m_abFixedBuffer);
	}
	operator T*() const throw()
	{
		return(m_p);
	}
	T* operator->() const throw()
	{
		assert(m_p != NULL);
		return(m_p);
	}

	T* Allocate(size_t nElements)
	{
		return(AllocateBytes(nElements*sizeof(T)));
	}

	T* Reallocate(size_t nElements)
	{
		size_t nNewSize = nElements*sizeof(T);

		if (m_p == NULL)
			return AllocateBytes(nNewSize);

		if (nNewSize > t_nFixedBytes)
		{
			if (m_p == reinterpret_cast< T* >(m_abFixedBuffer))
			{
				// We have to allocate from the heap and copy the contents into the new buffer
				AllocateHeap(nNewSize);
				memcpy(m_p, m_abFixedBuffer, t_nFixedBytes);
			}
			else
			{
				ReAllocateHeap(nNewSize);
			}
		}
		else
		{
			m_p = reinterpret_cast< T* >(m_abFixedBuffer);
		}

		return m_p;
	}

	T* AllocateBytes(size_t nBytes)
	{
		assert(m_p == NULL);
		if (nBytes > t_nFixedBytes)
		{
			AllocateHeap(nBytes);
		}
		else
		{
			m_p = reinterpret_cast< T* >(m_abFixedBuffer);
		}

		return(m_p);
	}

private:
	void AllocateHeap(size_t nBytes)
	{
		T* p = static_cast< T* >(Allocator::Allocate(nBytes));
		if (p == NULL)
		{
			throw std::bad_alloc();
		}
		m_p = p;
	}

	void ReAllocateHeap(size_t nNewSize)
	{
		T* p = static_cast< T* >(Allocator::Reallocate(m_p, nNewSize));
		if (p == NULL)
		{
			throw std::bad_alloc();
		}
		m_p = p;
	}

	void FreeHeap() throw()
	{
		Allocator::Free(m_p);
	}

private:
	T* m_p;
	char m_abFixedBuffer[t_nFixedBytes];
};



