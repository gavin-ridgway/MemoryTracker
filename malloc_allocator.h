
/** @file */
#ifndef MALLOC_ALLOCATOR_H_
#define MALLOC_ALLOCATOR_H_

#include <new>
#include <stdexcept>
#include <cstdlib>

/** @class malloc_allocator<T>
 * malloc_allocator: STL compatible allocator that uses malloc/free
 * from the Standard C library and NOT global new/delete.
 * 
 * This code derived from the article:
 * The Standard Librarian: What Are Allocators Good For?
 * by Matthew H. Austern, Dr. Dobbs, Dec 2000
 * 
 * An allocator almost identical to this also appears in GNU
 * libstdc++-v3, which states that it is identical to one
 * that appears in the C++ standard.
 * 
 */
template <class T> class malloc_allocator
{
public:
	typedef T                 value_type;
	typedef value_type*       pointer;
	typedef const value_type* const_pointer;
	typedef value_type&       reference;
	typedef const value_type& const_reference;
	typedef std::size_t       size_type;
	typedef std::ptrdiff_t    difference_type;

	template <class U> 
	struct rebind { typedef malloc_allocator<U> other; };

	malloc_allocator() {}
	malloc_allocator(const malloc_allocator&) {}
	template <class U> 
	malloc_allocator(const malloc_allocator<U>&) {}
	~malloc_allocator() {}

	pointer address(reference x) const { return &x; }
	const_pointer address(const_reference x) const { 
		return &x; 
	}
	
	pointer allocate(size_type n, const_pointer = 0) {
		void* p = std::malloc(n * sizeof(T));
		if (!p)
			throw std::bad_alloc();
		return static_cast<pointer>(p);
	}

	void deallocate(pointer p, size_type) {
		std::free(p);
	}

	size_type max_size() const { 
		return static_cast<size_type>(-1) / sizeof(value_type);
	}

	void construct(pointer p, const value_type& x) { 
		::new(p) value_type(x); 
	}
	void destroy(pointer p) { p->~value_type(); }

private:
	void operator=(const malloc_allocator&);
};


template<> class malloc_allocator<void>
{
  typedef void        value_type;
  typedef void*       pointer;
  typedef const void* const_pointer;

  template <class U> 
  struct rebind { typedef malloc_allocator<U> other; };
};


template <class T>
inline bool operator==(const malloc_allocator<T>&, 
                       const malloc_allocator<T>&) {
  return true;
}

template <class T>
inline bool operator!=(const malloc_allocator<T>&, 
                       const malloc_allocator<T>&) {
  return false;
}

#endif /*MALLOC_ALLOCATOR_H_*/
