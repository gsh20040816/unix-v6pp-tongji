// Support.c

#ifdef __cplusplus
extern "C" void _main()
#else
void _main()
#endif
{
	/* ELF toolchain path: run .init_array constructors first. */
	extern void (*__init_array_start[])();
	extern void (*__init_array_end[])();
	void (**initfn)() = __init_array_start;
	while ( initfn < __init_array_end )
	{
		if ( *initfn != 0 )
		{
			(*initfn)();
		}
		++initfn;
	}

	/* Compatibility path for legacy .ctors based objects. */
	extern void (*__CTOR_LIST__)();
	void (**constructor)() = &__CTOR_LIST__;
	while ( *constructor != 0 )
	{
		(*constructor)();
		++constructor;
	}
}

#ifdef __cplusplus
extern "C" void _atexit()
#else
void _atexit()
#endif
{
	/* ELF toolchain path: run .fini_array destructors. */
	extern void (*__fini_array_start[])();
	extern void (*__fini_array_end[])();
	void (**finifn)() = __fini_array_start;
	while ( finifn < __fini_array_end )
	{
		if ( *finifn != 0 )
		{
			(*finifn)();
		}
		++finifn;
	}

	/* Compatibility path for legacy .dtors based objects. */
	extern void (*__DTOR_LIST__)();
	void (**deconstructor)() = &__DTOR_LIST__;
	while ( *deconstructor != 0 )
	{
		(*deconstructor)();
		++deconstructor;
	}
}
