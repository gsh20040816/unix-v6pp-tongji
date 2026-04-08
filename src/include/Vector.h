#ifndef VECTOR_H
#define VECTOR_H

#include "Utility.h"

/*
 * 内核态最小 Vector。
 * 约束：
 * 1) 元素类型必须能默认构造并支持赋值；
 * 2) clear()/resize(缩小) 仅调整逻辑长度，不逐个析构元素；
 * 3) 适合 PageInfo 这类 POD 风格内核元数据，不追求完整 STL 语义。
 */
template <class T>
class Vector
{
public:
	Vector()
	{
		this->m_Data = NULL;
		this->m_Size = 0;
		this->m_Capacity = 0;
	}

	Vector(const Vector& other)
	{
		this->m_Data = NULL;
		this->m_Size = 0;
		this->m_Capacity = 0;

		if ( other.m_Size != 0 && this->reserve(other.m_Size) == false )
		{
			Utility::Panic("Out of kernel memory for Vector copy");
		}

		this->m_Size = other.m_Size;
		for ( unsigned int i = 0; i < other.m_Size; ++i )
		{
			this->m_Data[i] = other.m_Data[i];
		}
	}

	~Vector()
	{
		this->release();
	}

	Vector& operator=(const Vector& other)
	{
		if ( this == &other )
		{
			return *this;
		}

		if ( other.m_Size > this->m_Capacity )
		{
			T* newData = new T[other.m_Size];
			if ( newData == NULL )
			{
				Utility::Panic("Out of kernel memory for Vector assign");
			}

			delete [] this->m_Data;
			this->m_Data = newData;
			this->m_Capacity = other.m_Size;
		}

		this->m_Size = other.m_Size;
		for ( unsigned int i = 0; i < other.m_Size; ++i )
		{
			this->m_Data[i] = other.m_Data[i];
		}
		return *this;
	}

	unsigned int size() const
	{
		return this->m_Size;
	}

	unsigned int capacity() const
	{
		return this->m_Capacity;
	}

	bool empty() const
	{
		return this->m_Size == 0;
	}

	T& operator[](unsigned int index)
	{
		return this->m_Data[index];
	}

	const T& operator[](unsigned int index) const
	{
		return this->m_Data[index];
	}

	T* data()
	{
		return this->m_Data;
	}

	const T* data() const
	{
		return this->m_Data;
	}

	bool push_back(const T& value)
	{
		if ( this->m_Size == this->m_Capacity )
		{
			unsigned int newCapacity = this->m_Capacity == 0 ? 4 : this->m_Capacity * 2;
			if ( newCapacity < this->m_Capacity )
			{
				return false;
			}
			if ( this->reserve(newCapacity) == false )
			{
				return false;
			}
		}

		this->m_Data[this->m_Size++] = value;
		return true;
	}

	bool reserve(unsigned int newCapacity)
	{
		if ( newCapacity <= this->m_Capacity )
		{
			return true;
		}

		T* newData = new T[newCapacity];
		if ( newData == NULL )
		{
			return false;
		}

		for ( unsigned int i = 0; i < this->m_Size; ++i )
		{
			newData[i] = this->m_Data[i];
		}

		delete [] this->m_Data;
		this->m_Data = newData;
		this->m_Capacity = newCapacity;
		return true;
	}

	bool resize(unsigned int newSize)
	{
		if ( newSize > this->m_Capacity && this->reserve(newSize) == false )
		{
			return false;
		}

		if ( newSize > this->m_Size )
		{
			for ( unsigned int i = this->m_Size; i < newSize; ++i )
			{
				this->m_Data[i] = T();
			}
		}

		this->m_Size = newSize;
		return true;
	}

	void clear()
	{
		this->m_Size = 0;
	}

	void release()
	{
		if ( this->m_Data != NULL )
		{
			delete [] this->m_Data;
			this->m_Data = NULL;
		}
		this->m_Size = 0;
		this->m_Capacity = 0;
	}

private:
	T* m_Data;
	unsigned int m_Size;
	unsigned int m_Capacity;
};

#endif
